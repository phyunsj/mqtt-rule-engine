


```
=== Test Result ===
./unittest
  1 : "test_do_nothing" expected nothing : 1, "234"[3]
  2 : "test_convert_input_as_string" expected convert : 1, "{ 'humidity' : 9 }"[18]
  3 : "test_convert_input_as_number" expected convert : 1, "{ 'humidity' : 9 }"[18]
  4 : "test_convert_input_as_non_number" expected convert : 0, "{ 'humidity' : 0 }"[18]
  5 : "test_filter_input_as_string" expected filter : 0, "{ 'temperature' : 90 }"[22]
  6 : "test_filter_input_as_number" expected filter : 0, "{ 'temperature' : 80 }"[22]
  7 : "test_filter_input_as_non_number" expected filter : 0, "{ 'temperature' : 0 }"[21]
  8 : "test_filter_input_trigger_true" expected filter : 1, "{ 'temperature' : 120 }"[23]
  9 : "test_filter_compile_error" expected filter : 0, "attempt to call a nil value"[27]
 10 : "test_compiled_filter" expected filter : 1, "{ 'temperature' : 130 }"[23]
 11 : "create_database" expected SQLITE_OK
 12 : "insert_func_noaction_entry" expected SQLITE_OK
 13 : "insert_func_filter_entry" expected SQLITE_OK
 14 : "insert_func_convert_entry" expected SQLITE_OK
 15 : "insert_topic_noaction_entry" expected SQLITE_OK
 16 : "insert_topic_filter_entry" expected SQLITE_OK
 17 : "insert_topic_convert_entry" expected SQLITE_OK
 18 : "execute_non_topic_noaction" expected topic "city/building11/floor1/temperature" not found
 19 : "execute_topic_noaction" expected  <<< "noaction"[8] found >>>  noaction : 1, "120"[3]
 20 : "execute_topic_filter" expected  <<< "filter"[6] found >>>  filter : 0, "{ 'temperature' : 90 }"[22]
 21 : "execute_topic_convert" expected  <<< "convert"[7] found >>>  filter : 1, "{ 'humidity' : 9 }"[18]
ALL TESTS PASSED
Tests run: 21
```