
## gravizo diagram 


```
![Alt text](https://g.gravizo.com/svg?
@startuml;
hide footbox;
entity  Sensor;
participant "Publisher" as A;
participant "Broker" as B;
participant "Subscriber" as C;
...;
note over B: 1. Filter MQTT message;
Sensor -> A: Read Sensor;
A -> B: "85" Temperature F;
B->x B:;
Sensor -> A:;
A -> B: "95";
B->x B:;
Sensor -> A:;
A -> B: "110";
B -> C: if temp > "100" F;
@enduml;
)
```


```
![Alt text](https://g.gravizo.com/svg?
@startuml;
hide footbox;
entity  Sensor;
participant "Publisher" as A;
participant "Broker" as B;
participant "Subscriber" as C;
...;
note over B: 2. Convert MQTT message;
Sensor -> A:Read Sensor;
A -> B: "345" Range 0-2047;
B -> C: { "moisture" : "17"};
@enduml;
)
```

```
![Alt text](https://g.gravizo.com/svg?
@startuml;
hide footbox;
entity  Sensor;
participant "Publisher" as A;
participant "Broker" as B;
participant "Subscriber A" as C;
...;
note over B: 3. Distribute MQTT message;
Sensor -> A:Read Sensor ;
A -> B: "110" Temperature F;
B -> C: "110" Temperature F;
create "Subscriber B";
B --> "Subscriber B": warning if temp > 100 F ;
@enduml;
)

```