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


## Clients

### Web Client

We will have a very simple web client. Since we are not using HTTP and Web Sockets, we need to figure out how we will implemente this. The web client will be implemented in Flask.