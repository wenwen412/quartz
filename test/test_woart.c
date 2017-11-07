//
// Created by WenPan on 10/11/17.
//

#include "woart.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>
#include "flush_delay.h"

extern int extera_latency;

int fail_unless(int8_t conditiona)
{
    if (!conditiona)
    {
        printf ("Error");
        return 1;
    }
    return 0;
}

int test_art_insert()
{
    art_tree t;
    int res = art_tree_init(&t);
    //fail_unless(res == 0, "error, res != 0");

    int len;
    char buf[512];
    FILE *f = fopen("words.txt", "r");

    uintptr_t line = 1;
    while (fgets(buf, sizeof buf, f)) {
        printf("\n",line);
        len = strlen(buf);
        buf[len-1] = '\0';
        art_insert(&t, (unsigned char*)buf, len, (void*)line);
        //fail_unless(NULL == art_insert(&t, (unsigned char*)buf, len, (void*)line));
        if (art_size(&t) != line)
            return 1;
        //fail_unless(art_size(&t) == line);
        line++;
    }

    res = art_tree_destroy(&t);
    if(!res)
        return 1;
    //fail_unless(res == 0);
    return 0;
}


int test_art_insert_verylong()
{
    art_tree t;
    int res = art_tree_init(&t);
    fail_unless(res == 0);

    unsigned char key1[300] = {16,0,0,0,7,10,0,0,0,2,17,10,0,0,0,120,10,0,0,0,120,10,0,
                               0,0,216,10,0,0,0,202,10,0,0,0,194,10,0,0,0,224,10,0,0,0,
                               230,10,0,0,0,210,10,0,0,0,206,10,0,0,0,208,10,0,0,0,232,
                               10,0,0,0,124,10,0,0,0,124,2,16,0,0,0,2,12,185,89,44,213,
                               251,173,202,211,95,185,89,110,118,251,173,202,199,101,0,
                               8,18,182,92,236,147,171,101,150,195,112,185,218,108,246,
                               139,164,234,195,58,177,0,8,16,0,0,0,2,12,185,89,44,213,
                               251,173,202,211,95,185,89,110,118,251,173,202,199,101,0,
                               8,18,180,93,46,151,9,212,190,95,102,178,217,44,178,235,
                               29,190,218,8,16,0,0,0,2,12,185,89,44,213,251,173,202,
                               211,95,185,89,110,118,251,173,202,199,101,0,8,18,180,93,
                               46,151,9,212,190,95,102,183,219,229,214,59,125,182,71,
                               108,180,220,238,150,91,117,150,201,84,183,128,8,16,0,0,
                               0,2,12,185,89,44,213,251,173,202,211,95,185,89,110,118,
                               251,173,202,199,101,0,8,18,180,93,46,151,9,212,190,95,
                               108,176,217,47,50,219,61,134,207,97,151,88,237,246,208,
                               8,18,255,255,255,219,191,198,134,5,223,212,72,44,208,
                               250,180,14,1,0,0,8, '\0'};
    unsigned char key2[303] = {16,0,0,0,7,10,0,0,0,2,17,10,0,0,0,120,10,0,0,0,120,10,0,
                               0,0,216,10,0,0,0,202,10,0,0,0,194,10,0,0,0,224,10,0,0,0,
                               230,10,0,0,0,210,10,0,0,0,206,10,0,0,0,208,10,0,0,0,232,
                               10,0,0,0,124,10,0,0,0,124,2,16,0,0,0,2,12,185,89,44,213,
                               251,173,202,211,95,185,89,110,118,251,173,202,199,101,0,
                               8,18,182,92,236,147,171,101,150,195,112,185,218,108,246,
                               139,164,234,195,58,177,0,8,16,0,0,0,2,12,185,89,44,213,
                               251,173,202,211,95,185,89,110,118,251,173,202,199,101,0,
                               8,18,180,93,46,151,9,212,190,95,102,178,217,44,178,235,
                               29,190,218,8,16,0,0,0,2,12,185,89,44,213,251,173,202,
                               211,95,185,89,110,118,251,173,202,199,101,0,8,18,180,93,
                               46,151,9,212,190,95,102,183,219,229,214,59,125,182,71,
                               108,180,220,238,150,91,117,150,201,84,183,128,8,16,0,0,
                               0,3,12,185,89,44,213,251,133,178,195,105,183,87,237,150,
                               155,165,150,229,97,182,0,8,18,161,91,239,50,10,61,150,
                               223,114,179,217,64,8,12,186,219,172,150,91,53,166,221,
                               101,178,0,8,18,255,255,255,219,191,198,134,5,208,212,72,
                               44,208,250,180,14,1,0,0,8, '\0'};


    fail_unless(NULL == art_insert(&t, key1, 299, (void*)key1));
    fail_unless(NULL == art_insert(&t, key2, 302, (void*)key2));
    art_insert(&t, key2, 302, (void*)key2);
    fail_unless(art_size(&t) == 2);

    res = art_tree_destroy(&t);
    fail_unless(res == 0);
}


int test_art_insert_search(char * filename)
{
    art_tree t;
    clock_t start, finish;
    double duration;
    int res = art_tree_init(&t);
    fail_unless(res == 0);

    int len;
    char buf[512];
    FILE *f = fopen(filename, "r");

    uint64_t line = 1;
    start = clock();
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';
	/*
        fail_unless(NULL ==
                    art_insert(&t, (unsigned char*)buf, len, &line));*/
	art_insert(&t, (unsigned char*)buf, len, &line);
        line++;
    }
    uint64_t nlines = line - 1;
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "WOART Insert spends %f seconds\n", duration );

       //range query test
    fseek(f, 0, SEEK_SET);
    // Search for each line
    line = 1;
    start = clock();
    int i;
    char keys[10000][7];
    for (i =0; i <10000; i++)
    {
        int a = 1000000+i;
        sprintf(keys[i] ,"%ld", a);
    }
    for (i = 0; i <10000; i++)
    {
        uint64_t *val = (uint64_t*)(&t, (unsigned char*)keys[i], 8);
    }
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("clock cycle is %d\n",finish - start);
    printf( "WOART range query spends %f seconds\n", duration );

    //stats_report();
    // Seek back to the start
    fseek(f, 0, SEEK_SET);

    // Search for each line
    line = 1;
    start = clock();
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';

        uint64_t *val = (uint64_t*)art_search(&t, (unsigned char*)buf, len);
	/*
        if (fail_unless(line == *val))
        {
            printf("Line: %d Val: %" PRIuPTR " Str: %s\n", line,
                   val, buf);
        }
	*/
        line++;
    }
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "WOART search spends %f seconds\n", duration );

    // Updates
    fseek(f, 0, SEEK_SET);
    line = 1;
    start = clock();
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';
        fail_unless(NULL ==
                    art_insert(&t, (unsigned char*)buf, len, &line));
        line++;
    }
    nlines = line - 1;
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "WOART update spends %f seconds\n", duration );


    //check max/min
    start = clock();
    // Check the minimum
    art_leaf *l = art_minimum(&t);
    // Check the maximum
    l = art_maximum(&t);
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "WOART find max/min spends %f seconds\n", duration );

  
    // delete
    line = 1;
    fseek(f, 0, SEEK_SET);
    start = clock();
    uintptr_t val;
    uint64_t tree_nodes_num = art_size(&t);
    int repeart_keys = 0;
    while (fgets(buf, sizeof buf, f)) {

        len = strlen(buf);
        buf[len-1] = '\0';
        // Delete, should get lineno back
        //art_delete(&t, (unsigned char*)buf, len);
        val = (uintptr_t)art_delete(&t, (unsigned char*)buf, len);
        if(val ==0)
            repeart_keys ++;
        line++;
    }
    fail_unless(line == tree_nodes_num + repeart_keys + 1);
    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf("%d updates happend\n",repeart_keys);
    printf( "WOART delete spends %f seconds\n", duration );


    //res = art_tree_destroy(&t);
    //fail_unless(res == 0);
}

int test_art_insert_delete()
{
    art_tree t;
    int res = art_tree_init(&t);
    fail_unless(res == 0);

    int len;
    char buf[512];
    FILE *f = fopen("words.txt", "r");

    uintptr_t line = 1, nlines;
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';
        fail_unless(NULL ==
                    art_insert(&t, (unsigned char*)buf, len, (void*)line));
        line++;
    }

    nlines = line - 1;

    // Seek back to the start
    fseek(f, 0, SEEK_SET);

    // Search for each line
    line = 1;
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';

        // Search first, ensure all entries still
        // visible
        uintptr_t val = (uintptr_t)art_search(&t, (unsigned char*)buf, len);
        fail_unless(line == val);

        // Delete, should get lineno back
        val = (uintptr_t)art_delete(&t, (unsigned char*)buf, len);
        //fail_unless(line == val);

        // Check the size
        fail_unless(art_size(&t) == nlines - line);
        line++;
    }

    // Check the minimum and maximum
    fail_unless(!art_minimum(&t));
    fail_unless(!art_maximum(&t));

    res = art_tree_destroy(&t);
    fail_unless(res == 0);
}


int iter_cb(void *data, const unsigned char* key, uint32_t key_len, void *val) {
    uint64_t *out = (uint64_t*)data;
    uintptr_t line = (uintptr_t)val;
    uint64_t mask = (line * (key[0] + key_len));
    out[0]++;
    out[1] ^= mask;
    return 0;
}


int test_art_insert_iter()
{
    art_tree t;
    int res = art_tree_init(&t);
    fail_unless(res == 0);

    int len;
    char buf[512];
    FILE *f = fopen("words.txt", "r");

    uint64_t xor_mask = 0;
    uintptr_t line = 1, nlines;
    while (fgets(buf, sizeof buf, f)) {
        len = strlen(buf);
        buf[len-1] = '\0';
        fail_unless(NULL ==
                    art_insert(&t, (unsigned char*)buf, len, (void*)line));

        xor_mask ^= (line * (buf[0] + len));
        line++;
    }
    nlines = line - 1;

    uint64_t out[] = {0, 0};
    fail_unless(art_iter(&t, iter_cb, &out) == 0);

    fail_unless(out[0] == nlines);
    fail_unless(out[1] == xor_mask);

    res = art_tree_destroy(&t);
    fail_unless(res == 0);
}


int main(int argc, char **argv) {
    clock_t start, finish;
    double duration;
    start = clock();
    char * p;
    char * filename = argv[1];
    int conv = (int)strtol(argv[2], &p, 10);
    extra_latency = conv;
    //printf("conv is %d", conv);
    //test_flush_latency();
    //test_art_insert();
    //test_art_insert_verylong();
    test_art_insert_search(filename);
    //test_art_insert_delete();
    //test_art_insert_iter();

    finish = clock();
    duration = (double)(finish - start) / CLOCKS_PER_SEC;
    printf( "%f seconds\n", duration );
}

