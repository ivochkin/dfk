/**
 * @copyright
 * Copyright (c) 2016-2017 Stanislav Ivochkin
 * Licensed under the MIT License (see LICENSE)
 */


#include <dfk/list.h>
#include <dfk/internal.h>
#include <ut.h>


typedef struct {
  dfk_list_hook_t hook;
  int value;
} myint_t;


static void myint_init(myint_t* i, int value)
{
  i->value = value;
  dfk_list_hook_init(&i->hook);
}


typedef struct {
  dfk_list_t l;
  myint_t values[3];
} myint_fixture_t;


static void myint_fixture_setup(myint_fixture_t* f)
{
  size_t i;
  for (i = 0; i < DFK_SIZE(f->values); ++i) {
    myint_init(f->values + i, 10 + i);
  }
  dfk_list_init(&f->l);
}


static void myint_fixture_teardown(myint_fixture_t* f)
{
  DFK_UNUSED(f);
}


TEST(list, sizeof)
{
  EXPECT(dfk_list_sizeof() == sizeof(dfk_list_t));
}


TEST(list, iterator_sizeof)
{
  EXPECT(dfk_list_it_sizeof() == sizeof(dfk_list_it));
}


TEST(list, reverse_iterator_sizeof)
{
  EXPECT(dfk_list_rit_sizeof() == sizeof(dfk_list_rit));
}


TEST(list, init_free)
{
  dfk_list_t l;
  dfk_list_init(&l);
  EXPECT(dfk_list_size(&l) == 0);
}


TEST_F(myint_fixture, list, append)
{
  dfk_list_t* l = &fixture->l;
  dfk_list_append(l, &fixture->values[1].hook);
  EXPECT(dfk_list_size(l) == 1);
  dfk_list_it it;
  dfk_list_begin(l, &it);
  EXPECT(((myint_t* ) it.value)->value == 11);

  dfk_list_append(l, &fixture->values[0].hook);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 11);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 10);

  dfk_list_append(&fixture->l, &fixture->values[2].hook);
  EXPECT(dfk_list_size(l) == 3);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 11);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 12);
}


TEST_F(myint_fixture, list, prepend)
{
  dfk_list_t* l = &fixture->l;
  dfk_list_prepend(l, &fixture->values[1].hook);
  EXPECT(dfk_list_size(l) == 1);
  dfk_list_it it;
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 11);

  dfk_list_prepend(&fixture->l, &fixture->values[0].hook);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 11);

  dfk_list_prepend(&fixture->l, &fixture->values[2].hook);
  EXPECT(dfk_list_size(l) == 3);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 12);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 11);
}


TEST(list, erase_last)
{
  dfk_list_t l;
  myint_t v;

  myint_init(&v, 10);
  dfk_list_init(&l);
  dfk_list_append(&l, &v.hook);
  dfk_list_rit it;
  dfk_list_rbegin(&l, &it);
  dfk_list_rerase(&l, &it);
  EXPECT(dfk_list_size(&l) == 0);
}


TEST_F(myint_fixture, list, erase_from_head)
{
  size_t i;
  dfk_list_t* l = &fixture->l;
  for (i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  dfk_list_it it;
  dfk_list_begin(l, &it);
  dfk_list_erase(l, &it);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 11);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 12);
}


TEST_F(myint_fixture, list, erase_from_tail)
{
  dfk_list_t* l = &fixture->l;
  for (size_t i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  {
    dfk_list_rit it;
    dfk_list_rbegin(l, &it);
    dfk_list_rerase(l, &it);
    EXPECT(dfk_list_size(l) == 2);
  }
  dfk_list_it it;
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 11);
}


TEST_F(myint_fixture, list, erase_from_mid)
{
  dfk_list_t* l = &fixture->l;
  for (size_t i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  dfk_list_it it;
  dfk_list_begin(l, &it);
  dfk_list_it_next(&it);
  dfk_list_erase(l, &it);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 12);
}


TEST_F(myint_fixture, list, clear_non_empty)
{
  dfk_list_t* l = &fixture->l;
  for (size_t i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  dfk_list_clear(l);
  EXPECT(dfk_list_size(l) == 0);
}


TEST_F(myint_fixture, list, clear_empty)
{
  dfk_list_clear(&fixture->l);
  EXPECT(dfk_list_size(&fixture->l) == 0);
}


TEST_F(myint_fixture, list, pop_back)
{
  dfk_list_t* l = &fixture->l;
  for (size_t i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  dfk_list_pop_back(l);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_it it;
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 10);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 11);
}


TEST_F(myint_fixture, list, pop_front)
{
  dfk_list_t* l = &fixture->l;
  for (size_t i = 0; i < DFK_SIZE(fixture->values); ++i) {
    dfk_list_append(l, &fixture->values[i].hook);
  }
  dfk_list_pop_front(l);
  EXPECT(dfk_list_size(l) == 2);
  dfk_list_it it;
  dfk_list_begin(l, &it);
  EXPECT(((myint_t*) it.value)->value == 11);
  dfk_list_it_next(&it);
  EXPECT(((myint_t*) it.value)->value == 12);
}

