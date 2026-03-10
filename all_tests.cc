#include "derived_page/test/first_page_test.cc"
#include "derived_page/test/node_page_test.cc"
#include "gtest/gtest.h"
#include "os/test/os_test.cc"
#include "pager/test/pager_test.cc"
#include "utility/test/sql_rc_test.cc"
#include "btree/test/btree_student_test.cc"

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
