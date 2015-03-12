The resolv.conf file format
The resolv.conf format we recognize is a text file, each line of which should either be empty, contain a comment starting with the # character, or consist of a token followed zero or more arguments. The tokens we recognize are:

nameserver
Must be followed by the IP address of exactly one nameserver. As an extension, Libevent allows you to specify a nonstandard port for the nameserver, using the IP:Port or the [IPv6]:port syntax.

domain
The local domain name.

search
A list of names to search when resolving local hostnames. Any name that has fewer than "ndots" dots in it is considered local, and if we can’t resolve it as-is, we look in these domain names. For example, if "search" is example.com and "ndots" is 1, then when the user asks us to resolve "www", we will consider "www.example.com".

options
A space-separated list of options. Each option is given either as a bare string, or (if it takes an argument) in the option:value format. Recognized options are:

ndots:INTEGER
小于ndots时会去查询search域
Used to configure searching. See "search" above. Defaults to 1.

timeout:FLOAT
How long, in seconds, do we wait for a response from a DNS server before we assume we aren’t getting one? Defaults to 5 seconds.

max-timeouts:INT
How many times do we allow a nameserver to time-out in a row before we assume that it’s down? Defaults to 3.

max-inflight:INT
How many DNS requests do we allow to be pending at once? (If we try to do more requests than this, the extras will stall until the earlier ones are answered or time out.) Defaults to 64.

attempts:INT
How many times to we re-transmit a DNS request before giving up on it? Defaults to 3.

randomize-case:INT
If nonzero, we randomize the case on outgoing DNS requests and make sure that replies have the same case as our requests. This so-called "0x20 hack" can help prevent some otherwise simple active events against DNS. Defaults to 1.

bind-to:ADDRESS
If provided, we bind to the given address whenever we send packets to a nameserver. As of Libevent 2.0.4-alpha, it only applied to subsequent nameserver entries.

initial-probe-timeout:FLOAT
When we decide that a nameserver is down, we probe it with exponentially decreasing frequency to see if it has come back up. This option configures the first timeout in the series, in seconds. Defaults to 10.

getaddrinfo-allow-skew:FLOAT
When evdns_getaddrinfo() requests both an IPv4 address and an IPv6 address, it does so in separate DNS request packets, since some servers can’t handle both requests in one packet. Once it has an answer for one address type, it waits a little while to see if an answer for the other one comes in. This option configures how long to wait, in seconds. Defaults to 3 seconds.
