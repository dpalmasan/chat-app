# Chat App

## Backend

### Chat Service

The chat service is a stateful service that uses Web Sockets to maintain communication between clients and the service. It is implemented in C++.

#### Step 1

In this design we will write the boilerplate. For the first step the service will communicate with a client through web sockets. In this case, for now we will support only 1 on 1 chats. Later, we will expand to group chats. For 1 on 1 chats we will store the following:

* `message_id`: The id of the message
* `message_from`: Who sent the message to the user
* `message_to`: To who the user sent the mesasge
* `content`: Content of the message. Initially we will support only text
* `created_at`: The time creation of the message

For the data, we will just have a simple struct that holds the data sent from the client. We will also have a non-implemented function that writes the message to a data store. Let's the data store be abstract as it might have multiple implementations.

The service can handle multiple clients, for this I assume we use threads for a service instance, and we should keep track of the clients.


#### Step 2

Starting scale handling, as clients might be connected to different servers. We need an event driven mechanism, using synchronous queues to process the messages. We need a subscriber to the event that once the event is triggered, it writes it to the Data Store. The queue is visible for all the servers. Then we can poll messages from the data store. The message queue should be abstract, and later we can implement it using Kafka or some message queue provider.


#### Step 3

Simple authentication. Let's assume we have a service for authentication. There is a login endpoint where the user is logged in, and then starts the websocket connection. Let us initialize the data store with some test users, let's do 5.

We need to add to the data store a user object, which has the user id, the status (online/offline) and a last_active_at timestamp. We implement this as a presence server, the ws connection will check whether the use is present or not.

#### Step 4

Now we start getting to a production-ish stage. We implement a Data Store based on MongoDB. This is persistent, so even if we close the chat service/DB service, message queue and restart, we should be able to see the messages. It is already populated with the users for the presence server.


## Clients

### Specification

* We should be able to login as a user, we assume there is an authentication service that handles this, and the user has to exist in the chat service DB.
* On the left side we show all the users, and we divide into two segments: Online users and offline users. Bigger on the right side we show the messages from the selected chats. If no chat is selected we show nothing.

#### Web Client

##### Step 1

We will have a very simple web client. Since we are not using HTTP and Web Sockets, we need to figure out how we will implemente this. The web client will be implemented in Flask.

##### Step 2

We beautify the chat, we should show the message surrounded by an oval background, and the timestamp as a small text bottom right of the message (message time).

## Service Discovery

The app has to scale, so we have a load balancer to a service discovery, we route user requests and select the chat service instance based on the load. For the service discovery we will use Zookeeper. Once the server is selected for the user, we initiate the websocket to the selected server.