#!/bin/bash
# Start Liso HTTP server

PATH=/Users/XiaotongSun/Developer/cmu/network/http_server
HTTP_PORT=9999
HTTPS_PORT=10000
LOG_FILE=$PATH/lisod.log
LOCK_FILE=$PATH/lisod.lock
WWW_FOLDER=$PATH/www
CGI=$PATH/cgi_script.py
KEY_FILE=$PATH/15-441-project-1/xiaotons.key
CA=$PATH/15-441-project-1/xiaotons.crt

./lisod $HTTP_PORT $HTTPS_PORT $LOG_FILE $LOCK_FILE $WWW_FOLDER $CGI $KEY_FILE $CA
