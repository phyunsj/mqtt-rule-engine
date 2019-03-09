# MQTT Rule Engine

## Introduction

Exploring an idea to apply user-defined rule(s) on published MQTT messages. 

## Rule 

**Case 1.** Set the threshold (or high/low watermarks) value for a specific topic. Reduce unnecessary messages. 

<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%201.%20Filter%20MQTT%20message%3B%0ASensor%20-%3E%20A%3A%20Read%20Sensor%3B%0AA%20-%3E%20B%3A%20%2285%22%20Temperature%20F%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%2295%22%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%22110%22%3B%0AB%20-%3E%20C%3A%20if%20temp%20%3E%20%22100%22%20F%3B%0A%40enduml%3B"/>
<p>
 
Reviewed [Web Thing API Specification](https://iot.mozilla.org/wot/#example-2-a-more-complex-thing-description-with-semantic-annotations) to utilize `events` attribute to control IoT messages but not able to implement(or inject) a logic underneath with the current form. 

For example,
```
  "events": {
    "overheated": {
      "title": "Overheated",
      "@type": "OverheatedEvent",
      "type": "number",
      "unit": "degree celsius",
      ...
    }
  }
```

**Case 2.** Calculate (and/or Convert) to specific format. For example, [Soil Moisture Sensor](https://learn.sparkfun.com/tutorials/soil-moisture-sensor-hookup-guide?_ga=2.250722821.2011127879.1551811690-2119149971.1551811690) generates 0~880 when the sensor is dry (~0) and when it is completely saturated with moisture (~880).

<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%0A%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%202.%20Convert%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%3B%0AA%20-%3E%20B%3A%20%22345%22%20Range%200-2047%3B%0AB%20-%3E%20C%3A%20%7B%20%22moisture%22%20%3A%20%2217%22%7D%3B%0A%40enduml%3B"/>
<p>
 
**Case 3.** Wildcards (`+` single level,`#` multi level) are available but need more control over MQTT messasges. 


<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%20A%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%203.%20Distribute%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%20%3B%0AA%20-%3E%20B%3A%20%22110%22%20Temperature%20F%3B%0AB%20-%3E%20C%3A%20%22110%22%20Temperature%20F%3B%0Acreate%20%22Subscriber%20B%22%3B%0AB%20--%3E%20%22Subscriber%20B%22%3A%20warning%20if%20temp%20%3E%20100%20F%20%3B%0A%40enduml"/>
<p>
  

## Implementation

:pushpin: This is langualge/framework-specific example ( Eclipse mosquitto 1.5.7 MQTT broker + sqlite + lua). Therefore, the actual implemenation can be varied.

**Why Lua?** Lua provides _simple_, _embeddable scripting_ capabilities to work with C/C++ applications. Designing DSL(Domain-Specific-Language) might be another option. Alternatively, C/C++ interpreter like [Tiny C Compiler](https://bellard.org/tcc/) or [Ch : C/C++ interpreter](https://www.softintegration.com/products/raspberry-pi/) can be used.

Upon reviewing mosquitto sources, `lib/send_publish.c` is the ideal place to apply MQTT rules with minimum changes (My goal is to prove the concept rather than redesinging the existing package.) The updated version is available [here](https://github.com/phyunsj/mqtt-rule-engine/blob/master/send_publish.c). 

`mosquitto__rule_engine()` is called from `send__real_publish()`. Based on `topic`, either no rule or user created lua script is executed.

```
int send__real_publish(struct mosquitto *mosq, uint16_t mid, const char *topic, uint32_t payloadlen, const void *payload, int qos, bool retain, bool dup)
{
  ...
  // Apply rules to build "packet"
  if(payloadlen > 0 &&  mosquitto__rule_engine( topic, payload, mosquitto__packet_payload ) ) {  
  ...
```

`handler_publish()` from `src/handle_publish.c` manages new topic (insert new topic into topicTable if it doesn't exit). Initially it will be `noaction`.

#### [Test Results](https://github.com/phyunsj/mqtt-rule-engine/tree/master/unittest)

```
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
```

#### Related Posts:

- [Web Thing API Specification](https://iot.mozilla.org/wot/#example-2-a-more-complex-thing-description-with-semantic-annotations)
- [Eclipse Mosquitto - An open source MQTT broker](https://github.com/eclipse/mosquitto)
- [Tiny C Compiler](https://bellard.org/tcc/)
- [Ch : C/C++ interpreter](https://www.softintegration.com/products/raspberry-pi/)
- [Integerating Lua into C/C++](https://www.cs.usfca.edu/~galles/cs420/lecture/LuaLectures/LuaAndC.html)
- [MQTT Topics & Best Practices](https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices/) 

