#!/bin/bash
##get image from fish feeder camera server
wget 192.168.1.100/capture?cb_=0000 --output-document snap.jpg 
##push to file.io server, and get linkr
curl -F file=@snap.jpg https://file.io/|sed 's/[{}"]//g'|cut -d "," -f 6|sed "s/link.*://g"
##clear canera buffer
wget 192.168.1.100/capture?cb_=0000 --output-document snap.jpg 
wget 192.168.1.100/capture?cb_=0000 --output-document snap.jpg 
