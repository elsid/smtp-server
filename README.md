# smtp-server

Async SMTP server with multiprocess arhitecture.
Based on poll.
Has one main process and fixed number of workers.
Main process accepts connections and sends sockets to workers by round-robin scheduling.
