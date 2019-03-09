#!/usr/bin/python
#
# NOTE : DO NOT NAME YOUR UNITTEST SCRIPT AS "unittest.py". DELETE "unittest.pyc".
# You will see "AttributeError: 'module' object has no attribute 'TestCase'" Error.
#
import unittest
from mock import Mock
import paho.mqtt.client as mqtt
import sys
#import shutil
import time

def on_message(client, userdata, message):
    print("message received " ,str(message.payload.decode("utf-8")))
    print("message topic=",message.topic)
    print("message qos=",message.qos)
    print("message retain flag=",message.retain)
    print message


############
class mqtt_rule_test(unittest.TestCase):
    targetHost = "127.0.0.1"
    # Not used for this test. Focus on MQTT message payload from MQTT broker
    targetPort = 1883
    targetKeepalive =  60
    targetRetain = False
    targetQos = 0

    @classmethod
    def setUpClass(self):
        self.mock_callback = Mock()
        self.client = mqtt.Client("P1") #create new instance
        #self.client.on_message = on_message #attach function to callback
        self.client.on_message = self.mock_callback 
        self.client.connect(self.targetHost) #connect to broker
        self.client.loop_start() #start the loop
        print("Subscribing to topic","city/#")
        self.client.subscribe("city/#")

    @classmethod
    def tearDownClass(self):
        self.client.loop_stop() #stop the loop

    def setUp(self):
        pass

    def tearDown(self):
        pass

    def test_1_no_record_no_action(self):  
        self.mock_callback.reset_mock()    
        print("Publishing message to topic","city/building14/floor1/temperature")
        self.client.publish("city/building14/floor1/temperature","77")
        time.sleep(5)
        self.assertTrue(self.mock_callback.called)
        name, args, kwargs =  self.mock_callback.mock_calls[0] # or args, kwargs = self.mock_callback.call_args
        # args[2] : MQTT Message defined in https://github.com/eclipse/paho.mqtt.python/blob/master/src/paho/mqtt/client.py
        self.assertEqual( args[2].payload , "77")

    def test_2_no_action(self): 
        self.mock_callback.reset_mock()
        print("Publishing message to topic","city/building11/floor1/temperature")
        self.client.publish("city/building11/floor1/temperature","78")
        time.sleep(5)
        self.assertTrue(self.mock_callback.called)
        name, args, kwargs =  self.mock_callback.mock_calls[0] 
        # args[2] : MQTT Message
        self.assertEqual( args[2].payload , "78")

    def test_3_filter_ignore(self): 
        self.mock_callback.reset_mock()
        print("Publishing message to topic","city/building12/floor1/temperature")
        self.client.publish("city/building12/floor1/temperature","99")
        time.sleep(5)
        self.assertFalse(self.mock_callback.called)

    def test_4_filter_warn(self):
        self.mock_callback.reset_mock() 
        print("Publishing message to topic","city/building12/floor1/temperature")
        self.client.publish("city/building12/floor1/temperature","110")
        time.sleep(5)
        self.assertTrue(self.mock_callback.called)
        name, args, kwargs =  self.mock_callback.mock_calls[0] 
        # args[2] : MQTT Message
        self.assertEqual( args[2].payload , "{ 'temperature' : 110 }")

    def test_5_convert_to_percentage(self): 
        self.mock_callback.reset_mock()
        print("Publishing message to topic","city/building12/floor2/humidity")
        self.client.publish("city/building12/floor2/humidity","1012")
        time.sleep(5)
        self.assertTrue(self.mock_callback.called)
        name, args, kwargs =  self.mock_callback.mock_calls[0] 
        # args[2] : MQTT Message
        self.assertEqual( args[2].payload , "{ 'humidity' : 49 }")
        
if __name__ == '__main__':
    suite = unittest.TestLoader().loadTestsFromModule(sys.modules[__name__] )
    unittest.TextTestRunner(verbosity=3).run(suite)

