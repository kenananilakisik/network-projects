-----How to Run
- Receiver listens on port 15002, Sender listens on port 15000
- To run receiver: ./receiver -m <mode> -p 15000 -h <hostnamesender>
- To run sender: ./sender -m <mode> -p 15002 -h <hostnamereceiver> -f file
- mode = 1 means stop and wait, mode > 1 means go-back-n.
-----HL Approach
- Didn't use anything else than standard c library.
- Used non-blocking sockets to be able to send the packets in the window
independently from acks.
- Used time out and modified that time out to define go back n time out.
- Used struct to represent packets, which has data, sequence number, type
and datalength fields.

-----Challenges Faced
- Spent a lot of time figuring out how to set a timeout in c and figuring
out the optimal timeout value.

-----Tests
- Used both sender and receiver to generate faulty/out-of-order packets/acks.
- Tried with receiver not sending acks at all.
- Tried with sender sending out of order packets.
- Tried different kinds of files sush as .jpg,c .exe., .text.
- Observed the behaviour of the algorithm using both the command line and 
wireshark.

