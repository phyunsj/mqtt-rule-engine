
 
## UnitTest : lua script , sqlite select/insert/create

> $ make

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

#### SQLite Database for unittest

- `topicTable`

| MQTT topic (key)  | function name |
| ------------- | ------------- |
|city/building11/floor1/temperature|noaction|
|city/building12/floor1/temperature|filter|
|city/building12/floor2/humidity|convert||

- `funcTable`

| function name (key)  | function code |
| ------------- | ------------- |
|noaction| ... |
|filter| ... |
|convert| ... ||

```
$ sqlite3 code.db
SQLite version 3.24.0 2018-06-04 14:10:15
Enter ".help" for usage hints.

sqlite> select * from topicTable;
city/building11/floor1/temperature|noaction|
city/building12/floor1/temperature|filter|
city/building12/floor2/humidity|convert|

sqlite> select * from funcTable;
noaction|function action(a)
    return 1, a
end
filter|function action(a)
    if type(tonumber(a)) == "number" then
        local temperature = math.floor(tonumber(a))
        if temperature < 10 or temperature > 100 then
            return  1, "{ 'temperature' : "..tostring(temperature).." }"
        else
            return  0, "{ 'temperature' : "..tostring(temperature).." }"
        end
    else
        return 0, "{ 'temperature' : 0 }"
    end
end
convert|function action(a)
    if type(tonumber(a)) == "number" then
        local humidity = tonumber(a)
        humidity = math.floor(math.abs(humidity / 2048 * 100 ))
        return  1, "{ 'humidity' : "..tostring(humidity).." }"
    else
        return  0, "{ 'humidity' : 0 }"
    end
end
sqlite>
```

## UnitTest : MQTT Publish/Subscribe with Rules

#### Dependency

```
$ pip install paho-mqtt
$ pip install mock
```

#### Console Output

```
$ python --version
Python 2.7.15

$ python mqtt_rule_test.py
('Subscribing to topic', 'city/#')
test_1_no_record_no_action (__main__.mqtt_rule_test) ... ('Publishing message to topic', 'city/building14/floor1/temperature')
ok
test_2_no_action (__main__.mqtt_rule_test) ... ('Publishing message to topic', 'city/building11/floor1/temperature')
ok
test_3_filter_ignore (__main__.mqtt_rule_test) ... ('Publishing message to topic', 'city/building12/floor1/temperature')
ok
test_4_filter_warn (__main__.mqtt_rule_test) ... ('Publishing message to topic', 'city/building12/floor1/temperature')
ok
test_5_convert_to_percentage (__main__.mqtt_rule_test) ... ('Publishing message to topic', 'city/building12/floor2/humidity')
ok

----------------------------------------------------------------------
Ran 5 tests in 25.029s

OK
$
```

#### test script : mqtt_rule_test.py


```
   def test_4_filter_warn(self):
        self.mock_callback.reset_mock() 
        print("Publishing message to topic","city/building12/floor1/temperature")
        self.client.publish("city/building12/floor1/temperature","110")
        time.sleep(5)
        self.assertTrue(self.mock_callback.called)
        name, args, kwargs =  self.mock_callback.mock_calls[0] 
        # args[2] : MQTT Message
        self.assertEqual( args[2].payload , "{ 'temperature' : 110 }")
 ```
