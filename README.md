# no-title

## Introduction

[Web Thing API Specification](https://iot.mozilla.org/wot/#example-2-a-more-complex-thing-description-with-semantic-annotations)

```
  "events": {
    "overheated": {
      "title": "Overheated",
      "@type": "OverheatedEvent",
      "type": "number",
      "unit": "degree celsius",
      "description": "The lamp has exceeded its safe operating temperature",
      "links": [{"href": "/things/lamp/events/overheated"}]
    }
  }
```

## Rule 

<p align="center">
<img  width="700px" alt="Alt Text" src="https://g.gravizo.com/svg?%40startuml%3B%0Atitle%20Rule%20Examples%3B%0Ahide%20footbox%3B%0Aentity%20%20Sensor%3B%0Aparticipant%20%22Publisher%22%20as%20A%3B%0Aparticipant%20%22Broker%22%20as%20B%3B%0Aparticipant%20%22Subscriber%20A%22%20as%20C%3B%0A%7C%7C15%7C%7C%3B%0Anote%20over%20B%3A%201.%20Filter%20MQTT%20message%3B%0ASensor%20-%3E%20A%3A%20Read%20Sensor%3B%0AA%20-%3E%20B%3A%20%2285%22%20Temperature%20F%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%2295%22%3B%0AB-%3Ex%20B%3A%3B%0ASensor%20-%3E%20A%3A%3B%0AA%20-%3E%20B%3A%20%22110%22%3B%0AB%20-%3E%20C%3A%20if%20temp%20%3E%20100%20F%3B%0A%7C%7C45%7C%7C%3B%0A...%20...%3B%0A%7C%7C15%7C%7C%3B%0Anote%20over%20B%3A%202.%20Transform%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%3B%0AA%20-%3E%20B%3A%20%22345%22%20Range%200-2047%3B%0AB%20-%3E%20C%3A%20%2217%22%20Percent%3B%0A%7C%7C45%7C%7C%3B%0A...%3B%0A%7C%7C15%7C%7C%3B%0Anote%20over%20B%3A%203.%20Distribute%20MQTT%20message%3B%0ASensor%20-%3E%20A%3ARead%20Sensor%20%3B%0AA%20-%3E%20B%3A%20%22110%22%20Temperature%20F%3B%0AB%20-%3E%20C%3A%20%22110%22%20Temperature%20F%3B%0Acreate%20%22Subscriber%20B%22%3B%0AB%20--%3E%20%22Subscriber%20B%22%3A%20warning%20if%20temp%20%3E%20100%20F%20%3B%0A%7C%7C15%7C%7C%3B%0A%40enduml"/>
<p>



#### See Also

- [Eclipse Mosquitto - An open source MQTT broker](https://github.com/eclipse/mosquitto)
- [MQTT Topics & Best Practices](https://www.hivemq.com/blog/mqtt-essentials-part-5-mqtt-topics-best-practices/)
- [Tiny C Compiler](https://bellard.org/tcc/)
- [Integerating Lua into C/C++](https://www.cs.usfca.edu/~galles/cs420/lecture/LuaLectures/LuaAndC.html)
