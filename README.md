# MQTT Rule Engine

## Introduction

Exploring an idea to apply user-defined rule(s) on published MQTT messages. 

## Rule 

**Case 1.** Reduce unnecessary messages. 

<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%201.%20Filter%20MQTT%20message%3B%0ASensor%20-%3E%20A%3A%20Read%20Sensor%3B%0AA%20-%3E%20B%3A%20%2285%22%20Temperature%20F%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%2295%22%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%22110%22%3B%0AB%20-%3E%20C%3A%20if%20temp%20%3E%20%22100%22%20F%3B%0A%40enduml%3B"/>
<p>
 
:arrow_right: Set the threshold (or high/low watermarks) value for a specific topic.

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

**Case 2.** [Soil Moisture Sensor](https://learn.sparkfun.com/tutorials/soil-moisture-sensor-hookup-guide?_ga=2.250722821.2011127879.1551811690-2119149971.1551811690) generates 0~880 when the sensor is dry (~0) and when it is completely saturated with moisture (~880).

<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%0A%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%202.%20Convert%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%3B%0AA%20-%3E%20B%3A%20%22345%22%20Range%200-2047%3B%0AB%20-%3E%20C%3A%20%7B%20%22moisture%22%20%3A%20%2217%22%7D%3B%0A%40enduml%3B"/>
<p>
 
:arrow_right: Calculate (and/or Convert) to specific format.

**Case 3.** TBD

<p align="center">
<img  width="500px" alt="Alt Text" src="https://g.gravizo.com/svg?%40startuml%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%20A%22%20as%20C%3B%0A...%3B%0Anote%20over%20B%3A%203.%20Distribute%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%20%3B%0AA%20-%3E%20B%3A%20%22110%22%20Temperature%20F%3B%0AB%20-%3E%20C%3A%20%22110%22%20Temperature%20F%3B%0Acreate%20%22Subscriber%20B%22%3B%0AB%20--%3E%20%22Subscriber%20B%22%3A%20warning%20if%20temp%20%3E%20100%20F%20%3B%0A%40enduml"/>
<p>
  

## Design

:pushpin: This is langualge/framework-specific example. Therefore, the actual implemenation can be varied.

**Why Lua?** Lua provides _simple_, _embeddable scripting_ capabilities to work with C/C++ applications (e.g., mosquitto broker) Designing DSL(Domain-Specific-Language) might be another option. Alternatively, C/C++ interpreter like [Tiny C Compiler](https://bellard.org/tcc/) or [Ch : C/C++ interpreter](https://www.softintegration.com/products/raspberry-pi/) can be used.


TBD

#### Related Posts:

- [Web Thing API Specification](https://iot.mozilla.org/wot/#example-2-a-more-complex-thing-description-with-semantic-annotations)
- [Eclipse Mosquitto - An open source MQTT broker](https://github.com/eclipse/mosquitto)
- [MQTT Topics & Best Practices](https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices/)
- [Tiny C Compiler](https://bellard.org/tcc/)
- [Ch : C/C++ interpreter](https://www.softintegration.com/products/raspberry-pi/)
- [Integerating Lua into C/C++](https://www.cs.usfca.edu/~galles/cs420/lecture/LuaLectures/LuaAndC.html)
