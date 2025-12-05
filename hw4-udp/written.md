# 1 For each function, come up with the most appropriate description/comment block and include the function name and description in the written.pdf file.

static dp_connp dpinit()

Provides a zeroed out dp_connp struct for the main function to start filling out and default or always the same data,
such as the length of sockets. This makes it so the other functions don't have to
worry about setting lengths or worry about garbage memory, only the features it wants to change like sending to different servers.

void dpclose(dp_connp dpsession) 

Literally just frees the dp_connection struct, very simple

int  dpmaxdgram()

Returns the max size a drexel protocol datagram can store, pdu and data

dp_connp dpServerInit(int port) 

Grabs the drexel protocol connection information and then sets it up with the socket we got from the kernel. 
The socket in listens on all interfaces (which is what is given by default from dp_connp), but the socket out needs to
specify what address to send back to, which is why we finalized the socket in but not the socket out, we are still
waiting on the socket out information from recvfrom.
We also set the port we listen on and bind it according to params passed through.

dp_connp dpClientInit(char *addr, int port) 

Grabs the dp_connp from dpinit and then sets it up with the socket we got from the kernel. We set the port and address
to be that of the server we are trying to communicate with. The in and out socket are the set to be the same as
we know the return interface as opposed to viewing it through 0.0.0.0 and having to discover the return address.

int dprecv(dp_connp dp, void *buff, int buff_sz)

Returns the actual data inside the datagram, has nothing to do with the pdus or the protocol at all. Also returns if
connection is closed 

static int dprecvdgram(dp_connp dp, void *buff, int buff_sz)

Validates the pdu and sets up and sends the ack pdu. Sends back error codes if validation fails specifying why.

static int dprecvraw(dp_connp dp, void *buff, int buff_sz)

Gets the raw bytes sent to us and prints the pdu.

int dpsend(dp_connp dp, void *sbuff, int sbuff_sz)

performs a single size check to make sure we aren't sending too much data. This size check is redundant as we already
check this in dpsenddgram anyways

static int dpsenddgram(dp_connp dp, void *sbuff, int sbuff_sz)

Builds a pdu to wrap the data in, sends the data wrapped in the pdu and then recvs the ack and validates it.

static int dpsendraw(dp_connp dp, void *sbuff, int sbuff_sz)

Sends the bytes and prints out the pdu it sends

int dplisten(dp_connp dp) 

waits for a connect message and sends a connection back and an ack of the client connecting

int dpconnect(dp_connp dp) 

send appropriate connect pdu and wait for a return message that connects and acks our connect

int dpdisconnect(dp_connp dp) 

send appropriate disconnect message and wait for a return mess that disconnects and acks our disconnect

void * dp_prepare_send(dp_pdu *pdu_ptr, void *buff, int buff_sz) 

Currently isn't used in the codebase, wipes a buffer with 0's then puts the pdu passed through onto that buffer

void print_out_pdu(dp_pdu *pdu) 
void print_in_pdu(dp_pdu *pdu) 

preface the pdu with correct direction then print the common pdu field information

static void print_pdu_details(dp_pdu *pdu)

prints the contents of the pdu fields

static char * pdu_msg_to_string(dp_pdu *pdu) 

maps pdu message types in the struct (as a bit field) to their string equivalent

int dprand(int threshold)

generates true threshold % of the time for debugging and setting up test cases

# 2 I used a 3 sub-layers for various parts of transport model. If you look at dpsend() and dprecv(), each one of these is supported by two additional helper functions. For example dpsend() with dpsenddgram() and dpsendraw(). The same model is used by dprecv(). What are the specific responsibilities of these layers? Do you think this is a good design? If so, why. If not, how can it be improved?

I definitely see the use in the 3 layers, one is at the bottom for sending raw bytes, it is very useful to use in the
case of reusing an old pdu (the protocol itself can use this function for acks, but the user should call higher level
function), but the datagram one actually crafts the pdu for us which is also useful if we simply want
to send a brand new message. The plain recv and send (highest level, not raw and not dgram) seem kind of useless, they simply take a
buffer for the datagram recv or send functions and copy that buffer to the return buffer. 

I guess the point of the plain recv function at the highest level is if there is an error we don't change the buffer
which was passed into the function at all, we return the error code and dont copy the buffer from the next lowest level
(datagram function) to the returned buffer.

For the plain send function at the highest level I think maybe we could prevent side effects such as the seq number
going up from calling the datagram level incorrectly, but currently it does the same exact check in the datagram level
and the send level so this is pointless.

# 3 Describe how sequence numbers are used in the du-proto? Why do you think we update the sequence number for things that must be acknowledged (aka ACK response)?

The sequence numbers are currently shared between both the client and server streams, if the client sends data with a
sequence number, the server expects the sequence number to be the starting byte # of the clients sequence, the
server then increments its own sequence number by the same amount as the lenght of the client data, hence making its own
sequence number match, it will then send data with this sequence number and the client will have to increment its own
sequence number by the amount sent. They are always in sync. 

Also of note is that there is no checking that the sequence
numbers actually match, we simply increment our own sequence number by the offset of the length of data sent or
recieved. We check if we recieve an ack after a send but that ack doesn't mean that all the data was sent and the
sequence numer matches necessarily. I am thinking that the data is so small that it is all sent in one packet so nothing
can get lost so this wouldn't matter, except for the fact that the data can be corrupted or something. The only time the sequence
numbers sync up is on listen where the server uses the incoming pdu seqNum as their own seqNum, so not acking with a
seqNum means that if cient or server has a bug that they may not match up.

Any control data increases the sequence number by one even if it has no actual data. As there is no actual checking of
the seq number, just on every send we must receive an ack, this incrementation does nothing. Maybe it is useful in
debugging the program to see if our control messages are sent properly? This is my best guess

# 4 To keep things as simple as possible, the du-proto protocol requires that every send be ACKd before the next send is allowed. Can you think of at least one example of a limitation of this approach vs traditional TCP? Any insight into how this also simplified the implementation fo the du-proto protocol?

This causes a lot more messages to be sent then grouping acks together which is done to optimize TCP programs. It also
means that we can't allow tcp piggybacking (or i guess in this case du piggybacking), at least the way we have it set up now,
because the program expects a send ack back but wont process any data associated with the datagram. Overall this leads
to a higher percentage of control messages compared to actual data messages.

# 5 We looked at how to program with TCP sockets all term. This is the first example of the UDP programming interface we looked at. Breifly describe some of the differences associated with setting up and managing UDP sockets as compared to TCP sockets.

The biggest thing we do differently is use sendto and recvfrom instead of send and recv, means we don't have to send and
recieve to the same server each time, as we don't lock in the server with connect(). It also means we have the extra
step of having to determine where the message came from on recvfrom, and pass in where the message is to on sendto.
Besides this, we technically didn't have to recreate the transport layer of drexel protocol, many apps use udp without a
second transport layer like how we do, so this added overhead isn't directly from using UDP. If we didn't implement this
transport protocol we wouldn't know about messages which were sent but not recieved and the other reliability concerns
regarding TCP, but this isn't shown in the different in programming a UDP program.
