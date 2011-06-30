/*
 * This file is part of the Palacios Virtual Machine Monitor developed
 * by the V3VEE Project with funding from the United States National
 * Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  You can find out more at
 * http://www.v3vee.org
 *
 * Copyright (c) 2010, Lei Xia <lxia@northwestern.edu>
 * Copyright (c) 2010, The V3VEE Project <http://www.v3vee.org>
 * All rights reserved.
 *
 * This is free software.  You are permitted to use, redistribute,
 * and modify it under the terms of the GNU General Public License
 * Version 2 (GPLv2).  The accompanying COPYING file contains the
 * full text of the license.
 */
 
#ifndef __PALACIOS_HASHTABLE_H__
#define __PALACIOS_HASHTABLE_H__

struct hashtable;

#define __32BIT__

/* Example of use:
 *
 *      struct hashtable  *h;
 *      struct some_key   *k;
 *      struct some_value *v;
 *
 *      static uint_t         hash_from_key_fn( void *k );
 *      static int                  keys_equal_fn ( void *key1, void *key2 );
 *
 *      h = create_hashtable(16, hash_from_key_fn, keys_equal_fn);
 *      k = (struct some_key *)     malloc(sizeof(struct some_key));
 *      v = (struct some_value *)   malloc(sizeof(struct some_value));
 *
 *      (initialise k and v to suitable values)
 * 
 *      if (! hashtable_insert(h,k,v) )
 *      {     exit(-1);               }
 *
 *      if (NULL == (found = hashtable_search(h,k) ))
 *      {    printf("not found!");                  }
 *
 *      if (NULL == (found = hashtable_remove(h,k) ))
 *      {    printf("Not found\n");                 }
 *
 */

/* Macros may be used to define type-safe(r) hashtable access functions, with
 * methods specialized to take known key and value types as parameters.
 * 
 * Example:
 *
 * Insert this at the start of your file:
 *
 * DEFINE_HASHTABLE_INSERT(insert_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_SEARCH(search_some, struct some_key, struct some_value);
 * DEFINE_HASHTABLE_REMOVE(remove_some, struct some_key, struct some_value);
 *
 * This defines the functions 'insert_some', 'search_some' and 'remove_some'.
 * These operate just like hashtable_insert etc., with the same parameters,
 * but their function signatures have 'struct some_key *' rather than
 * 'void *', and hence can generate compile time errors if your program is
 * supplying incorrect data as a key (and similarly for value).
 *
 * Note that the hash and key equality functions passed to create_hashtable
 * still take 'void *' parameters instead of 'some key *'. This shouldn't be
 * a difficult issue as they're only defined and passed once, and the other
 * functions will ensure that only valid keys are supplied to them.
 *
 * The cost for this checking is increased code size and runtime overhead
 * - if performance is important, it may be worth switching back to the
 * unsafe methods once your program has been debugged with the safe methods.
 * This just requires switching to some simple alternative defines - eg:
 * #define insert_some hashtable_insert
 *
 */

typedef unsigned char uchar_t;
typedef unsigned int uint_t;
typedef unsigned long long ullong_t;
typedef unsigned long ulong_t;
typedef ulong_t addr_t;


#define DEFINE_HASHTABLE_INSERT(fnname, keytype, valuetype)		\
    static int fnname (struct hashtable * htable, keytype key, valuetype value) { \
	return v3_htable_insert(htable, (addr_t)key, (addr_t)value);	\
    }

#define DEFINE_HASHTABLE_SEARCH(fnname, keytype, valuetype)		\
    static valuetype * fnname (struct hashtable * htable, keytype  key) { \
	return (valuetype *) (v3_htable_search(htable, (addr_t)key));	\
    }

#define DEFINE_HASHTABLE_REMOVE(fnname, keytype, valuetype, free_key)	\
    static valuetype * fnname (struct hashtable * htable, keytype key) { \
	return (valuetype *) (v3_htable_remove(htable, (addr_t)key, free_key)); \
    }





/* These cannot be inlined because they are referenced as fn ptrs */
ulong_t palacios_hash_long(ulong_t val, uint_t bits);
ulong_t palacios_hash_buffer(uchar_t * msg, uint_t length);



struct hashtable * palacios_create_htable(uint_t min_size,
				    uint_t (*hashfunction) (addr_t key),
				    int (*key_eq_fn) (addr_t key1, addr_t key2));

void palacios_free_htable(struct hashtable * htable, int free_values, int free_keys);

/*
 * returns non-zero for successful insertion
 *
 * This function will cause the table to expand if the insertion would take
 * the ratio of entries to table size over the maximum load factor.
 *
 * This function does not check for repeated insertions with a duplicate key.
 * The value returned when using a duplicate key is undefined -- when
 * the hashtable changes size, the order of retrieval of duplicate key
 * entries is reversed.
 * If in doubt, remove before insert.
 */
int palacios_htable_insert(struct hashtable * htable, addr_t key, addr_t value);

int palacios_htable_change(struct hashtable * htable, addr_t key, addr_t value, int free_value);


// returns the value associated with the key, or NULL if none found
addr_t palacios_htable_search(struct hashtable * htable, addr_t key);

// returns the value associated with the key, or NULL if none found
addr_t palacios_htable_remove(struct hashtable * htable, addr_t key, int free_key);

uint_t palacios_htable_count(struct hashtable * htable);

// Specialty functions for a counting hashtable 
int palacios_htable_inc(struct hashtable * htable, addr_t key, addr_t value);
int palacios_htable_dec(struct hashtable * htable, addr_t key, addr_t value);


#endif
