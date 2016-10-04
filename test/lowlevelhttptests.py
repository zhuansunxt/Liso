#!/usr/bin/env python
import socket
from http_parser.pyparser import HttpParser

def check_correct_HEAD(host, port):
    #Check if HEAD only returns header but not body
    p = HttpParser()
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    rc_is_headers_complete = False
    rc_no_more_data = True
    try:
        s.connect((host, port))
        s.settimeout(1)
        s.send("HEAD /index.html HTTP/1.1\r\nHost: %s:%d\
            \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
        while True:
            data = s.recv(1024)

            if rc_is_headers_complete and data:
                rc_no_more_data = False
                break

            if not data:
                break

            recved = len(data)
            nparsed = p.execute(data, recved)
            assert nparsed == recved

            if p.is_headers_complete():
                rc_is_headers_complete = True

            if p.is_message_complete():
                break
    except socket.timeout:
        pass
    finally:
        s.close()
    return rc_is_headers_complete and rc_no_more_data

def check_correct_GET(host, port):
    #Check if only one response from each GET 
    error_reported = False
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    responses = 0
    buf_size = 1024
    data_all = ""
    try:
        s.connect((host, port))
        s.send("GET /index.html HTTP/1.1\r\nHost: %s:%d\
            \r\nConnection:Keep-Alive\r\n\r\n" % (host, port))
        while True:
            s.settimeout(1)
            data = s.recv(buf_size)
            data_all += data

            if not data:
                break
    except socket.timeout:
        pass
    finally:
        s.close()

    p = HttpParser()
    while len(data_all) > 0:
        nparsed = p.execute(data_all, len(data_all))
        if nparsed == 0:
            break
        if p.is_message_complete():
            responses += 1
            if nparsed < len(data_all):
                responses += 1 #more data 
            if p.get_status_code() >= 400:
                error_reported = True
            p = HttpParser() # create another
        data_all = data_all[nparsed:]

    return error_reported, responses

if __name__ == "__main__":
    print check_correct_HEAD("127.0.0.1", 6799)
    print check_correct_HEAD("eaufavor.info", 80)
    print check_correct_GET("127.0.0.1", 6799)
    print check_correct_GET("www.angio.net", 80)
