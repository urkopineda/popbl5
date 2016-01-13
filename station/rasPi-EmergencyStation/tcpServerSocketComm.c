//
//  tcpServerSocketComm.c
//  rasPi-EmergencyStation
//
//  Created by Gorka Olalde Mendia on 9/12/15.
//  Copyright © 2015 Gorka Olalde Mendia. All rights reserved.
//

#include "tcpServerSocketComm.h"

SSC_STAT serverSocketStat;
PMSGBUFF SSC_serverSendBuffer;

void SSC_initServerConnection() {
	int waitTime = 1;
	serverSocketStat.state = 1;
	serverSocketStat.buffer = calloc(SSC_SRV_BUFLEN, sizeof(char));
	serverSocketStat.sockSize = sizeof(struct sockaddr_in);
	serverSocketStat.serverInfo = NULL;
	serverSocketStat.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocketStat.serverSocket < 0) {
		perror("Error creating socket");
	}
	while (!serverSocketStat.serverInfo) {
		printf("Trying to search server IP address....\n");
		serverSocketStat.serverInfo = gethostbyname(SSC_SRV_ADDRESS);
		if (serverSocketStat.serverInfo == NULL) {
			perror("Host unreachable\n");
			sleep(waitTime);
			waitTime <<= 1;
		}
	}

	serverSocketStat.serverSocketStruct.sin_family = AF_INET;
	serverSocketStat.serverSocketStruct.sin_len = sizeof(struct sockaddr_in);
	memcpy(&serverSocketStat.serverSocketStruct.sin_addr, serverSocketStat.serverInfo->h_addr_list[0], serverSocketStat.serverInfo->h_length);
	serverSocketStat.serverSocketStruct.sin_port = htons(SSC_SRV_PORT);
	printf("Server IP Address: %s\n", inet_ntoa(serverSocketStat.serverSocketStruct.sin_addr));
	SSC_initBuffers();
	SSC_makeServerConnection();
	SSC_initServerConnThreads();
	
}
void SSC_initBuffers() {
	SSC_serverSendBuffer = MB_initBuffer(10);
	
	
}
void SSC_initServerConnThreads() {
	pthread_create(&serverSocketStat.listenThread, NULL, SSC_msgListenerThreadFunc, NULL);
	pthread_create(&serverSocketStat.sendThread, NULL, SSC_msgSenderThreadFunc, NULL);
}

void SSC_stopServerConn() {
	serverSocketStat.state = 0;
	shutdown(serverSocketStat.serverSocket, SHUT_RDWR);
	close(serverSocketStat.serverSocket);
	pthread_join(serverSocketStat.listenThread, NULL);
	pthread_join(serverSocketStat.sendThread, NULL);
}

void SSC_makeServerConnection() {
	int waitTime = 1;
	while (connect(serverSocketStat.serverSocket, (struct sockaddr *)&serverSocketStat.serverSocketStruct, serverSocketStat.sockSize) < 0) {
		perror("Error connecting to server");
		sleep(waitTime);
		waitTime <<= 1;
		close(serverSocketStat.serverSocket); //Advanced Programming in the Unix Enviorement, page 607, for testing in OSX, as after failed conexions, in *nix the socket is in an undefined state.
		serverSocketStat.serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocketStat.serverSocket < 0) {
			perror("Error creating socket");
		}
	}
	printf("Server Connected\n");
}

void SSC_sendMessageToServer(PMESSAGE msg) {
	send(serverSocketStat.serverSocket, msg->fullMsg, strlen(msg->fullMsg), 0);
}


void SSC_listenToServerMsg() {
	
	int msgLength = 0;
	PMESSAGE msg;
	struct sockaddr_in* clientSocketStruct;
	MP_PRECEIVERSTR receiver = calloc(1, sizeof(MP_RECEIVERSTR));
	receiver->clientBuff = serverSocketStat.buffer;
	receiver->bufferLength = SSC_SRV_BUFLEN;
	receiver->maxMsgLength = SSC_RCV_MAXLEN;
	while (((msgLength = recv(serverSocketStat.serverSocket, serverSocketStat.buffer,SSC_RCV_MAXLEN , 0)) > 0) && serverSocketStat.state == 1) {
		printf("Mensaje recibido de srv: %s, msgLength: %d\n", receiver->clientBuff, msgLength);
		receiver->msgLength = msgLength;
		msg = MP_messageReceiver(receiver);
		if (msg != NULL) {
			msg->source = 0;
			MB_putMessage(receivedMsgBuff, msg);
		}
	}
}

void* SSC_msgSenderThreadFunc(void* args) {
	PMESSAGE msg;
	while (serverSocketStat.state == 1) {
		msg = MB_getMessage(SSC_serverSendBuffer);
		SSC_sendMessageToServer(msg);
		printf("%s sent to server", msg->fullMsg);
		MP_wipeMessage(msg);
	}
	pthread_exit(NULL);
}

void* SSC_msgListenerThreadFunc(void* args) {
	while (serverSocketStat.state == 1) {
		SSC_listenToServerMsg();
		if (serverSocketStat.state == 1) {
			SSC_makeServerConnection();
		}
		
	}
	pthread_exit(NULL);
}