#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <assert.h>
#include "woart.h"
#include "flush_delay.h"
//#include "../src/lib/pmalloc.h"
#ifdef __i386__
#include <emmintrin.h>
#else
#ifdef __amd64__
#include <emmintrin.h>
#endif
#endif


#define LEAF_NODE (0)
#define LOG_NODE (1)
#define MAX_KEY_LEN (25)
#define VALUE_SIZE (4)
#define PM_NODE_TYPES (2)

#define LOG_MODE (0)


/**
 * Macros to manipulate pointer tags
 * Set last bit to 1 to identify a pointer to a LEAF
 * *********************************************************************
 * uintptr_t： an unsigned integer type with the property that any valid
 *  pointer to void can be converted to this type, then converted back
 *  to pointer to void, and the result will compare equal to the
 * original pointer
 */
#define IS_LEAF(x) (((uintptr_t)x & 1))
#define SET_LEAF(x) ((void*)((uintptr_t)x | 1))
#define LEAF_RAW(x) ((art_leaf*)((void*)((uintptr_t)x & ~1)))


/**********************************************************************
 * ********   pmalloc() and pfree() wrap psedo code  ******************
 */

void* pmalloc (size_t size){
    return malloc(size);
}

void pfree (void *ptr, size_t size) {
    return free(ptr);
}



void test_flush_latency()
{
    clock_t start, stop;
    start = clock();
    double duration;
    uint64_t *a = (uint64_t*)pmalloc(sizeof(uint64_t)*10000);
    start = clock();
    for (int i = 0; i < 10000; i++)
    {
	a[i] = i*i;
        _mm_clflush(a+i);
    }
    
    stop = clock();
    duration = (double)(stop - start) / CLOCKS_PER_SEC;
    printf("Flush latency latency is :%f", duration);
    printf("Flush latency latency is :%f", duration);
    pfree(a, sizeof(uint64_t)*10000);
    //free(a);
}


/* Slab allocation implementation
 * input: allocation type
 * return: a single data structure space*/
alloc_meta * tree_alloc_meta = NULL;



/**********************************************************************
********       art data structure code start here    *****************
**********************************************************************/
/**
 * Allocates a node of the given type,
 * initializes to zero and sets the type.
 */
static art_node* alloc_node(uint8_t type) {
    art_node* n;
    switch (type) {
        case NODE4:
            n = (art_node*)pmalloc(sizeof(art_node4));
            memset(n, 0, sizeof(art_node4));
            //change for WOART
            //((art_node4*)n)->keys_slot = (unsigned char *)pmalloc(sizeof(unsigned char)*8);
            //memset(((art_node4*)n)->keys_slot, 0, sizeof(unsigned char)*8);
            break;
        case NODE16:
            n = (art_node*)pmalloc(sizeof(art_node16));
            memset(n, 0, sizeof(art_node16));
            break;
        case NODE48:
            n = (art_node*)pmalloc(sizeof(art_node48));
            memset(n, 0, sizeof(art_node48));
            break;
        case NODE256:
            n = (art_node*)pmalloc(sizeof(art_node256));
            memset(n, 0, sizeof(art_node256));
            break;
        default:
            abort();
    }
    n->type = type;
    return n;
}

/**
 * Initializes an ART tree
 * @return 0 on success.
 * fixme: log_head point to a log type with no data
 */
int art_tree_init(art_tree *t) {
    t->root = NULL;
    t->leaf_head = NULL;

    art_log * head;
    head = (art_log *)malloc(sizeof(art_log));
    if (head==NULL)
        return 0;
    head->next = NULL;
    head->leaf = NULL;

    t->log_head = head;

    t->size = 0;
    printf("init return 0");
    return 0;
}

// Recursively destroys the tree
static void destroy_node(art_node *n) {
    // Break if null
    if (!n) return;

    // Special case leafs
    if (IS_LEAF(n)) {
        pfree(LEAF_RAW(n), sizeof(art_node));
        return;
    }

    // Handle each node type
    int i;
    union {
        art_node4 *p1;
        art_node16 *p2;
        art_node48 *p3;
        art_node256 *p4;
    } p;
    switch (n->type) {
        case NODE4:
            p.p1 = (art_node4*)n;
            //FOR WOART, need to free slot array space
            //pfree(p.p1->keys_slot, sizeof(unsigned char)*8);
            for (i=0;i<n->num_children;i++) {
                destroy_node(p.p1->children[i]);
            }

            break;

        case NODE16:
            p.p2 = (art_node16*)n;
            for (i=0;i<n->num_children;i++)  {
                destroy_node(p.p2->children[i]);
            }
            break;

        case NODE48:
            p.p3 = (art_node48*)n;
            for (i=0;i<n->num_children;i++) {
                destroy_node(p.p3->children[i]);
            }
            break;

        case NODE256:
            p.p4 = (art_node256*)n;
            for (i=0;i<256;i++) {
                if (p.p4->children[i])
                    destroy_node(p.p4->children[i]);
            }
            break;

        default:
            abort();
    }

    // Free ourself on the way up
    pfree(n, sizeof(art_node));
}



/**
 * Destroys an ART tree
 * @return 0 on success.
 */
int art_tree_destroy(art_tree *t) {
    printf("Destory node\n");
    destroy_node(t->root);
    t->leaf_head = NULL;
    return 0;
}

/**
 * Returns the size of the ART tree.
 */

#ifndef BROKEN_GCC_C99_INLINE
extern inline uint64_t art_size(art_tree *t);
#endif

static art_node** find_child(art_node *n, unsigned char c) {
    int i, mask, bitfield;
    union {
        art_node4 *p1;
        art_node16 *p2;
        art_node48 *p3;
        art_node256 *p4;
    } p;
    switch (n->type) {
        case NODE4:
            p.p1 = (art_node4*)n;
            for (i=0 ; i < n->num_children; i++) {
                /* this cast works around a bug in gcc 5.1 when unrolling loops
                 * https://gcc.gnu.org/bugzilla/show_bug.cgi?id=59124
                 */
                /* For WOART*/
                if (((unsigned char*)p.p1->keys_slot)[i] == c)
                {
                    int idx = ((unsigned char*)p.p1->keys_slot)[i + 4];
                    return &p.p1->children[idx];
                }

            }
            break;

            {
                case NODE16:
                    p.p2 = (art_node16*)n;
                /*****************************
                 * WOART didn't sort the keys/pointers
                 * Need a sqeuential search*/
                for (i = 0; i < n->num_children; i++)
                {
                    if(p.p2->keys[i] == c)
                        return &p.p2->children[i];
                }
/*
                // support non-86 architectures
#ifdef __i386__
                // Compare the key to all 16 stored keys
                __m128i cmp;
                cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
                        _mm_loadu_si128((__m128i*)p.p2->keys));

                // Use a mask to ignore children that don't exist
                mask = (1 << n->num_children) - 1;
                bitfield = _mm_movemask_epi8(cmp) & mask;
#else
#ifdef __amd64__
                // Compare the key to all 16 stored keys
                __m128i cmp;
                cmp = _mm_cmpeq_epi8(_mm_set1_epi8(c),
                                     _mm_loadu_si128((__m128i*)p.p2->keys));

                // Use a mask to ignore children that don't exist
                mask = (1 << n->num_children) - 1;
                bitfield = _mm_movemask_epi8(cmp) & mask;
#else
                // Compare the key to all 16 stored keys
                bitfield = 0;
                for (i = 0; i < 16; ++i) {
                    if (p.p2->keys[i] == c)
                        bitfield |= (1 << i);
                }

                // Use a mask to ignore children that don't exist
                mask = (1 << n->num_children) - 1;
                bitfield &= mask;
#endif
#endif
*/
                /*
                 * If we have a match (any bit set) then we can
                 * return the pointer match using ctz to get
                 * the index.
                 */
                /*
                if (bitfield)
                    return &p.p2->children[__builtin_ctz(bitfield)];*/
                break;
            }

        case NODE48:
            p.p3 = (art_node48*)n;
            i = p.p3->keys[c];
            if (i)
                return &p.p3->children[i-1];
            break;

        case NODE256:
            p.p4 = (art_node256*)n;
            if (p.p4->children[c])
                return &p.p4->children[c];
            break;

        default:
            abort();
    }
    return NULL;
}

// Simple inlined if
static inline int min(int a, int b) {
    return (a < b) ? a : b;
}

/**
 * Returns the number of prefix characters shared between
 * the key and node.
 */
static int check_prefix(const art_node *n, const unsigned char *key, int key_len, int depth) {
    int max_cmp = min(min(n->partial_len, MAX_PREFIX_LEN), key_len - depth);
    int idx;
    for (idx=0; idx < max_cmp; idx++) {
        if (n->partial[idx] != key[depth+idx])
            return idx;
    }
    return idx;
}

/**
 * Checks if a leaf matches
 * @return 0 on success.
 */
static int leaf_matches(const art_leaf *n, const unsigned char *key, int key_len, int depth) {
    (void)depth;
    // Fail if the key lengths are different
    if (n->key_len != (uint32_t)key_len) return 1;

    // Compare the keys starting at the depth
    return memcmp(n->key, key, key_len);
}

/**
 * Searches for a value in the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 */
void* art_search(const art_tree *t, const unsigned char *key, int key_len) {
    art_node **child;
    art_node *n = t->root;
    int prefix_len, depth = 0;
    while (n) {
        // Might be a leaf
        if (IS_LEAF(n)) {
            n = (art_node*)LEAF_RAW(n);
            // Check if the expanded path matches
            if (!leaf_matches((art_leaf*)n, key, key_len, depth)) {
                return ((art_leaf*)n)->value;
            }
            return NULL;
        }

        // Bail if the prefix does not match
        if (n->partial_len) {
            prefix_len = check_prefix(n, key, key_len, depth);
            if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len))
                return NULL;
            depth = depth + n->partial_len;
        }

        // Recursively search
        child = find_child(n, key[depth]);
        n = (child) ? *child : NULL;
        depth++;
    }
    return NULL;
}

// Find the minimum leaf under a node
static art_leaf* minimum(const art_node *n) {
    // Handle base cases
    if (!n) return NULL;
    if (IS_LEAF(n)) return LEAF_RAW(n);

    int idx;
    switch (n->type) {
        case NODE4:
            return minimum(((const art_node4*)n)->children[0]);
        case NODE16:
            return minimum(((const art_node16*)n)->children[0]);
        case NODE48:
            idx=0;
            while (!((const art_node48*)n)->keys[idx]) idx++;
            idx = ((const art_node48*)n)->keys[idx] - 1;
            return minimum(((const art_node48*)n)->children[idx]);
        case NODE256:
            idx=0;
            while (!((const art_node256*)n)->children[idx]) idx++;
            return minimum(((const art_node256*)n)->children[idx]);
        default:
            abort();
    }
}

// Find the maximum leaf under a node
static art_leaf* maximum(const art_node *n) {
    // Handle base cases
    if (!n) return NULL;
    if (IS_LEAF(n)) return LEAF_RAW(n);

    int idx;
    switch (n->type) {
        case NODE4:
            return maximum(((const art_node4*)n)->children[n->num_children-1]);
        case NODE16:
            return maximum(((const art_node16*)n)->children[n->num_children-1]);
        case NODE48:
            idx=255;
            while (!((const art_node48*)n)->keys[idx]) idx--;
            idx = ((const art_node48*)n)->keys[idx] - 1;
            return maximum(((const art_node48*)n)->children[idx]);
        case NODE256:
            idx=255;
            while (!((const art_node256*)n)->children[idx]) idx--;
            return maximum(((const art_node256*)n)->children[idx]);
        default:
            abort();
    }
}

/**
 * Returns the minimum valued leaf
 */
art_leaf* art_minimum(art_tree *t) {
    return minimum((art_node*)t->root);
}

/**
 * Returns the maximum valued leaf
 */
art_leaf* art_maximum(art_tree *t) {
    return maximum((art_node*)t->root);
}


static art_leaf* make_leaf(art_log **log_head, const unsigned char *key, int key_len, void *value) {
    art_leaf *l = (art_leaf*)pmalloc(sizeof(art_leaf)+key_len);
    l->value = (void *)pmalloc(sizeof(uint64_t));
    memcpy(l->value, value, sizeof(uint64_t));
    persistent(l->value, sizeof(uint64_t), 1);

    memcpy(l->key, key, key_len);
    persistent(l->key, MAX_KEY_LEN,1);
    l->key_len = key_len;
    persistent(&(l->key_len), sizeof(uint32_t),1);
    return l;
}

/**
 * l1:abcdefg hkkfvk
 * l2:ssddefg hkklva
 * 			 ^
 * 			 |
 * 			depth
 *
 * compare start at depth, find common prefix
 * return the length of common prefix
 * */
static int longest_common_prefix(art_leaf *l1, art_leaf *l2, int depth) {
    int max_cmp = min(l1->key_len, l2->key_len) - depth;
    int idx;
    for (idx=0; idx < max_cmp; idx++) {
        if (l1->key[depth+idx] != l2->key[depth+idx])
            return idx;
    }
    return idx;
}

static void copy_header(art_node *dest, art_node *src) {
    dest->num_children = src->num_children;
    dest->partial_len = src->partial_len;
    memcpy(dest->partial, src->partial, min(MAX_PREFIX_LEN, src->partial_len));
    persistent(&(dest->num_children), sizeof(uint8_t), 1);
    persistent(&(dest->partial_len), sizeof(uint32_t), 1);
    persistent(dest->partial, sizeof(unsigned char)*min(MAX_PREFIX_LEN, src->partial_len), 1);
}

static void add_child256(art_node256 *n, art_node **ref, unsigned char c, void *child) {
    (void)ref;
    n->children[c] = (art_node*)child;
    persistent(&(n->children[c]), sizeof(art_node *), 2);
    n->n.num_children++;
    persistent(&(n->n.num_children), sizeof(uint8_t), 1);

}

static void add_child48(art_node48 *n, art_node **ref, unsigned char c, void *child) {
    if (n->n.num_children < 48) {
        int pos = 0;
        while (n->children[pos]) pos++;
        n->children[pos] = (art_node*)child;
        persistent(n->children[pos], sizeof(art_node *), 2);
        n->keys[c] = pos + 1;
        persistent(&(n->keys[c]), sizeof(char), 1);
        n->n.num_children++;
        persistent(&(n->n.num_children), sizeof(uint8_t), 1);
    } else {
        art_node256 *new_node = (art_node256*)alloc_node(NODE256);
        for (int i=0;i<256;i++) {
            if (n->keys[i]) {
                new_node->children[i] = n->children[n->keys[i] - 1];
                persistent(&(new_node->children[i]), sizeof(art_node *), 2);
            }
        }
        copy_header((art_node*)new_node, (art_node*)n);
        *ref = (art_node*)new_node;
        pfree(n, sizeof(art_node48));
        add_child256(new_node, ref, c, child);
    }
}

static void add_child16(art_node16 *n, art_node **ref, unsigned char c, void *child) {
    if (n->n.num_children < 16) {
        /***************************************
         * WOART use append only method, no need to sort*/
        unsigned  idx;
        idx = n->n.num_children;

        // Set the child
        n->keys[idx] = c;
        n->children[idx] = (art_node*)child;
        PERSISTENT_BARRIER();
        persistent(&(n->keys[idx]), sizeof(char), 0);
        persistent(&(n->children[idx]), sizeof(art_node *), 1);
        n->n.num_children++;
        PERSISTENT_BARRIER();
        persistent(&(n->n.num_children), sizeof(uint8_t), 1);
    } else {
        art_node48 *new_node = (art_node48*)alloc_node(NODE48);

        // Copy the child pointers and populate the key map
        memcpy(new_node->children, n->children,
               sizeof(void*)*n->n.num_children);
        persistent(new_node->children, sizeof(void*)*n->n.num_children, 1);
        for (int i=0;i<n->n.num_children;i++) {
            new_node->keys[n->keys[i]] = i + 1;
        }
        persistent(new_node->keys, sizeof(char) * n->n.num_children, 1);
        copy_header((art_node*)new_node, (art_node*)n);
        *ref = (art_node*)new_node;
        pfree(n, sizeof(art_node16));
        add_child48(new_node, ref, c, child);
    }
}

/*keys are sorted*/
static void add_child4(art_node4 *n, art_node **ref, unsigned char c, void *child) {
    int idx1, idx2;
    if (n->n.num_children < 4) {
        idx1 = n->n.num_children;

        n->children[idx1] = (art_node*)child;

        /*WOART insert key and children in a append-only manner*/
        PERSISTENT_BARRIER();
        persistent(&(n->children[idx1]), sizeof(art_node *),1);

        for (idx2=0; idx2 < n->n.num_children; idx2++) {
            if (c < n->keys_slot[idx2]) break;
        }
        unsigned char tmp_keys_slot[8];
	    memcpy(tmp_keys_slot, n->keys_slot, sizeof(tmp_keys_slot));
        /*for (int i = 0; i < 8; ++i) {
            tmp_keys_slot[i] = n->keys_slot[i];
        }*/

        // Shift to make room
        //memmove()) is a safe version of memcpy()
        //shifting both keys and pointers to the right
        memmove(tmp_keys_slot+idx2+1, tmp_keys_slot+idx2, n->n.num_children - idx2);
        memmove(tmp_keys_slot+idx2+1+4, tmp_keys_slot+idx2+4, n->n.num_children - idx2);
        tmp_keys_slot[idx2] = c;
        tmp_keys_slot[idx2 + 4] = (unsigned char)(idx1);
	    memcpy(n->keys_slot, tmp_keys_slot, sizeof(tmp_keys_slot));
        PERSISTENT_BARRIER();
        persistent(n->keys_slot, sizeof(char)*8,1);
        n->n.num_children++;
        persistent(&(n->n.num_children), sizeof(char)*8,1);
    } else {
        art_node16 *new_node = (art_node16*)alloc_node(NODE16);

        // Copy the child pointers and the key map

        memcpy(new_node->children, n->children,
               sizeof(void*)*n->n.num_children);
        persistent(new_node->children, sizeof(void*)*n->n.num_children, 1);
        /*******************************************
          *FOR WOART: need to change the order of children
          * Or the order of keys
          */
        int idx;
        for (int i = 0; i < n->n.num_children; i++)
        {
            idx = n->keys_slot[i+4];
            new_node->keys[idx] = n->keys_slot[i];       
        }
        persistent(new_node->keys, sizeof(new_node->keys),1);
        
//        memcpy(new_node->keys, n->keys_slot,
//               sizeof(unsigned char)*n->n.num_children);
        copy_header((art_node*)new_node, (art_node*)n);
        *ref = (art_node*)new_node;
        persistent(ref, sizeof(art_node **), 1);
        //pfree(n->keys_slot, sizeof(unsigned char)*8);
        //n->keys_slot = NULL;
        pfree(n, sizeof(art_node4));
        n = NULL;
        add_child16(new_node, ref, c, child);
    }
}

static void add_child(art_node *n, art_node **ref, unsigned char c, void *child) {
    switch (n->type) {
        case NODE4:
            return add_child4((art_node4*)n, ref, c, child);
        case NODE16:
            return add_child16((art_node16*)n, ref, c, child);
        case NODE48:
            return add_child48((art_node48*)n, ref, c, child);
        case NODE256:
            return add_child256((art_node256*)n, ref, c, child);
        default:
            abort();
    }
}

/**
 * Calculates the index at which the prefixes mismatch
 * Specificilly. When no match found, return 0;
 * Which means when partial_len == 0, this is not a compressed node
 */
static int prefix_mismatch(const art_node *n, const unsigned char *key, int key_len, int depth) {
    int max_cmp = min(min(MAX_PREFIX_LEN, n->partial_len), key_len - depth);
    int idx;
    for (idx=0; idx < max_cmp; idx++) {
        if (n->partial[idx] != key[depth+idx])
            return idx;
    }

    // If the prefix is short we can avoid finding a leaf
    if (n->partial_len > MAX_PREFIX_LEN) {
        // Prefix is longer than what we've checked, find a min leaf l
        //and get  the prefix of key and l
        art_leaf *l = minimum(n);
        max_cmp = min(l->key_len, key_len)- depth;
        for (; idx < max_cmp; idx++) {
            if (l->key[idx+depth] != key[depth+idx])
                return idx;
        }
    }
    return idx;
}


/**
 * Insert a leaf node into the double-linked-list
 * The operation is atomic safe
 * fixme: has to add mechanism to prevent persistent memory leak*/
static void linklist_insert(art_log *log_head, art_leaf **leaf_header, art_leaf *node)
{
    /***********************
     * WOART don't use this*/
    return;
}


/** n: the tree the are insert to
 * ref: the tree node that has index to the inserted node n
 * key, key_len, value: you know it
 * depth: current depth
 * old: a flag incicates we find an existing leaf and update it
 * return value: pointer to old_val when update is performaned (
 * find same key already exists in the radix tree)
 * return 0 when create a new node
 * */
static void* recursive_insert(art_tree *t, art_node *n, art_node **ref,
                              art_leaf **leaf_header, art_log **log_header,
                              const unsigned char *key, int key_len, void *value, int depth, int *old) {
    // If we are at a NULL node, inject a leaf
    art_node *tmp;
    if (!n) {
        tmp = (art_node*)SET_LEAF(make_leaf(log_header, key, key_len, value));
        // leaf_header = LEAF_RAW (*ref);
        // fixme: need to prevent memory leak
        linklist_insert(*log_header, leaf_header, LEAF_RAW (tmp));
        *ref = tmp;
        return NULL;
    }

    // If we are at a leaf, we need to replace it with a node
    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);

        // Check if we are updating an existing value, depth not used
        // fixme: use delete + insert to update a existing leaf
        // leaf_matches() returns 0 when match
        if (!leaf_matches(l, key, key_len, depth)) {
            void * new_value = (void *) pmalloc(sizeof(uint64_t));
            pfree(l->value, sizeof(uint64_t));
            l->value = new_value;
            memcpy(l->value, value, sizeof(uint64_t));
            persistent(l->value, sizeof(uint64_t), 1);
            *old = 1;
            return NULL;
        }

        // New key, we must split the leaf into a node4
        art_node4 *new_node = (art_node4*)alloc_node(NODE4);

        // Create a new leaf
        art_leaf *l2 = make_leaf(log_header, key, key_len, value);

        // Determine longest prefix
        int longest_prefix = longest_common_prefix(l, l2, depth);
        //In a compressed node, partial_len is set to be prefix length
        new_node->n.partial_len = longest_prefix;
        memcpy(new_node->n.partial, key+depth, min(MAX_PREFIX_LEN, longest_prefix));
        // Add the leafs to the new node4
        *ref = (art_node*)new_node;
        add_child4(new_node, ref, l->key[depth+longest_prefix], SET_LEAF(l));
        //Wen Pan: insert into linked list
        linklist_insert(*log_header, leaf_header, l2);
        add_child4(new_node, ref, l2->key[depth+longest_prefix], SET_LEAF(l2));
        return NULL;
    }

    // Check if given node has a prefix
    if (n->partial_len) {
        // Determine if the prefixes differ, since we need to split
        // Calculates the index at which the prefixes mismatch
        int prefix_diff = prefix_mismatch(n, key, key_len, depth);
        //which means there are child nodes, still need to compare
        if ((uint32_t)prefix_diff >= n->partial_len) {
            depth += n->partial_len;
            goto RECURSE_SEARCH;
        }

        // Create a new inner node, which contains the common parts (prefix)
        art_node4 *new_node = (art_node4*)alloc_node(NODE4);
        *ref = (art_node*)new_node;
        new_node->n.partial_len = prefix_diff;
        memcpy(new_node->n.partial, n->partial, min(MAX_PREFIX_LEN, prefix_diff));

        // Adjust the prefix of the old node
        if (n->partial_len <= MAX_PREFIX_LEN) {
            add_child4(new_node, ref, n->partial[prefix_diff], n);
            n->partial_len -= (prefix_diff+1);
            memmove(n->partial, n->partial+prefix_diff+1,
                    min(MAX_PREFIX_LEN, n->partial_len));
        } else {
            n->partial_len -= (prefix_diff+1);
            art_leaf *l = minimum(n);
            add_child4(new_node, ref, l->key[depth+prefix_diff], n);
            memcpy(n->partial, l->key+depth+prefix_diff+1,
                   min(MAX_PREFIX_LEN, n->partial_len));
        }

        // Insert the new leaf
        art_leaf *l = make_leaf(log_header, key, key_len, value);
        //Wen Pan: insert into linked list
        linklist_insert(*log_header, leaf_header, LEAF_RAW (l));
        add_child4(new_node, ref, key[depth+prefix_diff], SET_LEAF(l));
        return NULL;
    }

    RECURSE_SEARCH:;

    // Find a child to recurse to
    art_node **child = find_child(n, key[depth]);
    if (child) {
        return recursive_insert(t,*child, child,leaf_header, log_header,
                                key, key_len, value, depth+1, old);
    }

    // No child, node goes within us
    art_leaf *l = make_leaf(log_header, key, key_len, value);
    //Wen Pan: insert into linked list
    linklist_insert(*log_header, leaf_header, LEAF_RAW (l));
    add_child(n, ref, key[depth], SET_LEAF(l));
    return NULL;
}

/**
 * Inserts a new value into the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @arg value Opaque value.
 * @return NULL if the item was newly inserted, otherwise
 * the old value pointer is returned.
 */
void* art_insert(art_tree *t, const unsigned char *key, int key_len, void *value) {
    int old_val = 0;
    void *old = recursive_insert(t, t->root, &t->root, &t->leaf_head,
                                 &t->log_head, key, key_len, value, 0, &old_val);
    if (!old_val) t->size++;
    return old;
}

static void remove_child256(art_node256 *n, art_node **ref, unsigned char c) {
    n->children[c] = NULL;
    n->n.num_children--;

    // Resize to a node48 on underflow, not immediately to prevent
    // trashing if we sit on the 48/49 boundary
    if (n->n.num_children == 37) {
        art_node48 *new_node = (art_node48*)alloc_node(NODE48);
        *ref = (art_node*)new_node;
        copy_header((art_node*)new_node, (art_node*)n);

        int pos = 0;
        for (int i=0;i<256;i++) {
            if (n->children[i]) {
                new_node->children[pos] = n->children[i];
                new_node->keys[i] = pos + 1;
                pos++;
            }
        }
        pfree(n, sizeof(art_node256));
    }
}

static void remove_child48(art_node48 *n, art_node **ref, unsigned char c) {
    int pos = n->keys[c];
    n->keys[c] = 0;
    n->children[pos-1] = NULL;
    n->n.num_children--;

    if (n->n.num_children == 12) {
        art_node16 *new_node = (art_node16*)alloc_node(NODE16);
        *ref = (art_node*)new_node;
        copy_header((art_node*)new_node, (art_node*)n);

        int child = 0;
        for (int i=0;i<256;i++) {
            pos = n->keys[i];
            if (pos) {
                new_node->keys[child] = i;
                new_node->children[child] = n->children[pos - 1];
                child++;
            }
        }
        pfree(n, sizeof(art_node48));
    }
}

static void remove_child16(art_node16 *n, art_node **ref, art_node **l) {
    int pos = l - n->children;
    memmove(n->keys+pos, n->keys+pos+1, n->n.num_children - 1 - pos);
    memmove(n->children+pos, n->children+pos+1, (n->n.num_children - 1 - pos)*sizeof(void*));
    n->n.num_children--;

    if (n->n.num_children == 3) {
        art_node4 *new_node = (art_node4*)alloc_node(NODE4);
        *ref = (art_node*)new_node;
        copy_header((art_node*)new_node, (art_node*)n);
        memcpy(new_node->keys_slot, n->keys, 4);
        for (int i = 0; i <= n->n.num_children; ++i) {
            new_node->keys_slot[i+4] = i;
        }
        memcpy(new_node->children, n->children, 4*sizeof(void*));
        pfree(n, sizeof(art_node16));
    }
}

static void remove_child4(art_node4 *n, art_node **ref, art_node **l) {
    //For WOART, chid pos is different from key pos
    int child_pos = l - n->children;
    int key_pos;
    for (key_pos=0; key_pos<3 ; key_pos++)
    {
        if(n->keys_slot[4+key_pos] == child_pos)
            break;
    }
    //child_pos = n->keys_slot[key_pos+4];
    memmove(n->keys_slot+key_pos, n->keys_slot+key_pos+1, n->n.num_children - 1 - key_pos);
    memmove(n->keys_slot+key_pos+4, n->keys_slot+key_pos+5, n->n.num_children - 1 - key_pos);

    //memmove(n->children+child_pos, n->children+child_pos+1, (n->n.num_children - 1 - child_pos)*sizeof(void*));
    //n->children[child_pos] = NULL;
    n->n.num_children--;

    // Remove nodes with only a single child
    if (n->n.num_children == 1) {
        //For WOART
        child_pos = n->keys_slot[4];
        art_node *child = n->children[child_pos];
        if (!IS_LEAF(child)) {
            // Concatenate the prefixes
            int prefix = n->n.partial_len;
            if (prefix < MAX_PREFIX_LEN) {
                n->n.partial[prefix] = n->keys_slot[0];
                prefix++;
            }
            if (prefix < MAX_PREFIX_LEN) {
                int sub_prefix = min(child->partial_len, MAX_PREFIX_LEN - prefix);
                memcpy(n->n.partial+prefix, child->partial, sub_prefix);
                prefix += sub_prefix;
            }

            // Store the prefix in the child
            memcpy(child->partial, n->n.partial, min(prefix, MAX_PREFIX_LEN));
            child->partial_len += n->n.partial_len + 1;
        }
        *ref = child;
	// FIXME: if pfree n->keys_slot here, some mbind problem happens in Quartz
        //pfree(n->keys_slot, sizeof(unsigned char) * 8);
	//n->keys_slot = NULL;
        pfree(n, sizeof(art_node4));
    }
}

/**
 * Remove a pointer/key pair from the inner node
 * If the size of the node is shrinked into certain boundary,
 * Change the type of the node (e.g., node16->node4)
 * */
static void remove_child(art_node *n, art_node **ref, unsigned char c, art_node **l) {
    switch (n->type) {
        case NODE4:
            return remove_child4((art_node4*)n, ref, l);
        case NODE16:
            return remove_child16((art_node16*)n, ref, l);
        case NODE48:
            return remove_child48((art_node48*)n, ref, c);
        case NODE256:
            return remove_child256((art_node256*)n, ref, c);
        default:
            abort();
    }
}


/**
 * return l if successfully find the leaf node*/
static art_leaf* recursive_delete(art_node *n, art_node **ref, const unsigned char *key, int key_len, int depth) {
    // Search terminated
    if (!n) return NULL;

    // Handle hitting a leaf node
    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);
        if (!leaf_matches(l, key, key_len, depth)) {
            *ref = NULL;
            return l;
        }
        return NULL;
    }

    // Bail if the prefix does not match
    if (n->partial_len) {
        int prefix_len = check_prefix(n, key, key_len, depth);
        if (prefix_len != min(MAX_PREFIX_LEN, n->partial_len)) {
            return NULL;
        }
        depth = depth + n->partial_len;
    }

    // Find child node
    art_node **child = find_child(n, key[depth]);
    if (!child) return NULL;

    // If the child is leaf, delete from this node
    if (IS_LEAF(*child)) {
        art_leaf *l = LEAF_RAW(*child);
        if (!leaf_matches(l, key, key_len, depth)) {
            remove_child(n, ref, key[depth], child);
            return l;
        }
        return NULL;

        // Recurse
    } else {
        return recursive_delete(*child, child, key, key_len, depth+1);
    }
}

/**Wen Pan: Remove a leaf from double-linkerd_list
 * Considerations:
 * 	1. Consistency
 * 	2. Recover
 *
 * A special case： leaf is the header
 * */
int remove_leaf_from_list(art_tree *t, art_leaf *l){

    return 0;
}

void *delete_log(art_tree *t,const unsigned char *key){
    return NULL;

}


/**
 * Deletes a value from the ART tree
 * @arg t The tree
 * @arg key The key
 * @arg key_len The length of the key
 * @return NULL if the item was not found, otherwise
 * the value pointer is returned.
 *
 * fixme: after deletion, the node should be gone
 * and return an old value is no possible;
 * Thus, user program shoul dexpect a different return value of this call
 */
void* art_delete(art_tree *t, const unsigned char *key, int key_len) {
    art_leaf *l = recursive_delete(t->root, &t->root, key, key_len, 0);
    if (l) {
        t->size--;
        void *old = l->value;
        pfree(l->value, sizeof(uint64_t));
        pfree(l, sizeof(art_leaf) + l->key_len);
        return (void *)1;
    }
    return NULL;
}

// Recursively iterates over the tree
static int recursive_iter(art_node *n, art_callback cb, void *data) {
    // Handle base cases
    if (!n) return 0;
    if (IS_LEAF(n)) {
        art_leaf *l = LEAF_RAW(n);
        return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
    }

    int idx, res;
    switch (n->type) {
        case NODE4:
            for (int i=0; i < n->num_children; i++) {
                res = recursive_iter(((art_node4*)n)->children[i], cb, data);
                if (res) return res;
            }
            break;

        case NODE16:
            for (int i=0; i < n->num_children; i++) {
                res = recursive_iter(((art_node16*)n)->children[i], cb, data);
                if (res) return res;
            }
            break;

        case NODE48:
            for (int i=0; i < 256; i++) {
                idx = ((art_node48*)n)->keys[i];
                if (!idx) continue;

                res = recursive_iter(((art_node48*)n)->children[idx-1], cb, data);
                if (res) return res;
            }
            break;

        case NODE256:
            for (int i=0; i < 256; i++) {
                if (!((art_node256*)n)->children[i]) continue;
                res = recursive_iter(((art_node256*)n)->children[i], cb, data);
                if (res) return res;
            }
            break;

        default:
            abort();
    }
    return 0;
}

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each. The call back gets a
 * key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter(art_tree *t, art_callback cb, void *data) {
    return recursive_iter(t->root, cb, data);
}

/**
 * Checks if a leaf prefix matches
 * @return 0 on success.
 */
static int leaf_prefix_matches(const art_leaf *n, const unsigned char *prefix, int prefix_len) {
    // Fail if the key length is too short
    if (n->key_len < (uint32_t)prefix_len) return 1;

    // Compare the keys
    return memcmp(n->key, prefix, prefix_len);
}

/**
 * Iterates through the entries pairs in the map,
 * invoking a callback for each that matches a given prefix.
 * The call back gets a key, value for each and returns an integer stop value.
 * If the callback returns non-zero, then the iteration stops.
 * @arg t The tree to iterate over
 * @arg prefix The prefix of keys to read
 * @arg prefix_len The length of the prefix
 * @arg cb The callback function to invoke
 * @arg data Opaque handle passed to the callback
 * @return 0 on success, or the return of the callback.
 */
int art_iter_prefix(art_tree *t, const unsigned char *key, int key_len, art_callback cb, void *data) {
    art_node **child;
    art_node *n = t->root;
    int prefix_len, depth = 0;
    while (n) {
        // Might be a leaf
        if (IS_LEAF(n)) {
            n = (art_node*)LEAF_RAW(n);
            // Check if the expanded path matches
            if (!leaf_prefix_matches((art_leaf*)n, key, key_len)) {
                art_leaf *l = (art_leaf*)n;
                return cb(data, (const unsigned char*)l->key, l->key_len, l->value);
            }
            return 0;
        }

        // If the depth matches the prefix, we need to handle this node
        if (depth == key_len) {
            art_leaf *l = minimum(n);
            if (!leaf_prefix_matches(l, key, key_len))
                return recursive_iter(n, cb, data);
            return 0;
        }

        // Bail if the prefix does not match
        if (n->partial_len) {
            prefix_len = prefix_mismatch(n, key, key_len, depth);

            // Guard if the mis-match is longer than the MAX_PREFIX_LEN
            if ((uint32_t)prefix_len > n->partial_len) {
                prefix_len = n->partial_len;
            }

            // If there is no match, search is terminated
            if (!prefix_len) {
                return 0;

                // If we've matched the prefix, iterate on this node
            } else if (depth + prefix_len == key_len) {
                return recursive_iter(n, cb, data);
            }

            // if there is a full match, go deeper
            depth = depth + n->partial_len;
        }

        // Recursively search
        child = find_child(n, key[depth]);
        n = (child) ? *child : NULL;
        depth++;
    }
    return 0;
}


