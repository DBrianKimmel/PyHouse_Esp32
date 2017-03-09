MQTT Library for Esp32
======================

by D. Brian Kimmel


API
---

Mqtt_init
	Called first
	returns OK or out of memory
	Creates and initializes storage


Mqtt_start
	Called if init returned OK
	Uses transport to connect to broker
	Issues Mqtt connect message and processes
	Issues Subscribe message and processes


Tasks
-----

Transport Connect
	Called to connect the transport network to the broker.
	If the connection is broken, short delay and reconnect.

