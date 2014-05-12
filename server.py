#!/usr/bin/python
from socket import *
import json

s = socket()
s.bind(('localhost', 3333))
s.listen(4)
ns, na = s.accept()
print("Connected")

i = 0
while 1:

    try:
        data = ns.recv(8192)
    except:
        break

    for line in data.split('\n'):
	if not line.strip():
     	# Skip last line which doesn't contain any message
	    continue
	message = json.loads(line)
        print message
	if message['id'] == 1 and message['method'] == "mining.subscribe" :
	    print("Got subscribe request")
	    ns.send("{\"id\": 1, \"result\": [[[\"mining.set_difficulty\", \"03000000\"], [\"mining.notify\", \"ae6812eb4cd7735a302a8a9dd95cf71f\"]], \"08000002\", 4], \"error\": null}\n")
	if message['id'] == 2 and message['method'] == "mining.authorize" :
	    print("Got login request")
	    ns.send("{\"error\": null, \"id\": 2, \"result\": true}\n")
	    ns.send("{\"params\": [\"800\", \"4d16b6f85af6e2198f44ae2a6de67f78487ae5611b77c6c0440b921e00000000\"," +
		    "\"01000000010000000000000000000000000000000000000000000000000000000000000000ffffffff20020862062f503253482f04b8864e5008\"," +
		    "\"072f736c7573682f000000000100f2052a010000001976a914d23fcdf86f7e756a64a7a9688ef9903327048ed988ac00000000\", []," + 
		    "\"00000002\", \"1c2ac4af\", \"504e86b9\", false], \"id\": null, \"method\": \"mining.notify\"}\n")
ns.close()
s.close()


