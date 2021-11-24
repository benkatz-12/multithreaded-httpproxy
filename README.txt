# Network Systems - CSCI 4273
Files:
    ./makefile -- makefile to compile program and clean working directory
        usage:
            make -- compile program
            make clean -- clean working directory of temporary files
    ./webproxy.c -- main sourcecode
    ./webproxy.h -- main headerfile
    ./webproxy_f.c -- helper functions for main sourcecode
    ./blacklist.txt -- text file for adding domains and IPs to be blocked
        usage:
            put one domain/IP per line
    ./cache/ -- folder for temporary cached files
    ./README.md -- this page


Usage:
    To compile: make
    To run proxy:  ./webproxy <port> <cache timeout>
        port: the port you would like the proxy to run on
        cache timeout: the number of second before an item in the cache is considered invalid

Synopsis:
    This web proxy only deals with http traffic
    It features: 
        a blacklist to block outgoing requests 
        a page cache to speed up response time along with a variable expiration
        a hostname-ip cache to speed up request time
    The proxy accepts http web traffic and forwards that trafic to the desired destination.
    Upon recipt of the requested item, it caches the page and names it as the md5sum of the page URI
        This is for O(1) lookup time
    I edit the connection from 'keep-alive' to 'close' to save thread resources

Flow:
    Once an incoming connection is accepted, the proxy spawns a new thread to handle the connection
    The thread parses the connection into a serv_conn struct
    It checks the hostname against the blacklist and sends a '403 Forbidden' response if there is a hit
    It then checks the hostname against the hostname-IP cache to save on DNS lookup time
        If there is a hit, there is no need for DNS lookup
    After we have the IP, we check it aginst the blacklist again
    If there are no issues, we check the cache for the file
        If the file is present, we check the file descriptor for the time last modified and
            compare that with the current machine time. If this value is less than the variable timeout
            then we send the cached file
    If we do not send the cached file, then open a connection to the requested server and start the proxy service
    In the proxy service, we build and send a new http request for the requested item
    Upon recipt of data from the server, we write it out to both the client and the cache at the same time
        We use the content-length http header to get the length of the incoming response
            so anything using chunked content encoding will not function correct
    After the response is forwareded to the client and copied into the cache, the thread exits.
    
