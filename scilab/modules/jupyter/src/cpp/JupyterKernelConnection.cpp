/*
 *  Scilab ( http://www.scilab.org/ ) - This file is part of Scilab
 *  Copyright (C) 2016 - Leonardo José Consoni
 *
 * Copyright (C) 2012 - 2016 - Scilab Enterprises
 *
 * This file is hereby licensed under the terms of the GNU GPL v2.0,
 * pursuant to article 5.3.4 of the CeCILL v.2.1.
 * This file was originally licensed under the terms of the CeCILL v2.1,
 * and continues to be available under such terms.
 * For more information, see the COPYING file which you should have received
 * along with this program.
 *
 */

#include "JupyterKernelConnection.hxx"

#include "uuid.hxx"

#include <iostream>
#include <chrono>       // C++11 time handling functions

zmq::context_t JupyterKernelConnection::context( 1 );


JupyterKernelConnection::JupyterKernelConnection( std::string transport, std::string ipHost, std::string ioPubPort, std::string controlPort,
                                                  std::string stdinPort, std::string shellPort, std::string heartbeatPort ) 
: ioPubSocket( context, ZMQ_PUB ), 
controlSocket( context, ZMQ_ROUTER ),
stdinSocket( context, ZMQ_ROUTER ),
shellSocket( context, ZMQ_ROUTER )
{
  sessionUUID = GenerateNewUUID();
  
  // Binding I/O Pub socket to its port
  ioPubSocket.bind( transport + "://" + ipHost + ":" + ioPubPort );
  
  // Binding Control socket to its port
  controlSocket.bind( transport + "://" + ipHost + ":" + controlPort );
  
  // Binding Stdin socket to its port
  stdinSocket.bind( transport + "://" + ipHost + ":" + stdinPort );
  
  // Binding Shell socket to its port
  shellSocket.bind( transport + "://" + ipHost + ":" + shellPort );
  
  // Overloaded (void*) cast operator. Returns underlying socket_t::ptr member
  requestsPoller[ 0 ].socket = (void*) ioPubSocket;
  requestsPoller[ 0 ].events = ZMQ_POLLIN;
  requestsPoller[ 1 ].socket = (void*) controlSocket;
  requestsPoller[ 1 ].events = ZMQ_POLLIN;
  requestsPoller[ 2 ].socket = (void*) stdinSocket;
  requestsPoller[ 2 ].events = ZMQ_POLLIN;
  requestsPoller[ 3 ].socket = (void*) shellSocket;
  requestsPoller[ 3 ].events = ZMQ_POLLIN;
  
  heartbeatThread = std::thread( RunHeartbeatLoop, transport, ipHost, heartbeatPort, &isRunning );
}

JupyterKernelConnection::~JupyterKernelConnection()
{
  Shutdown();
}

void JupyterKernelConnection::SetHashGenerator( std::string keyString, std::string signatureScheme )
{
  messageHashGenerator.SetKey( keyString, signatureScheme );
}

void JupyterKernelConnection::ReceiveMessages( std::queue<JupyterMessage>& messageInQueue )
{    
  zmq::socket_t* socketsList[ 4 ] = { &ioPubSocket, &controlSocket, &stdinSocket, &shellSocket };
  
  // Poll sockets. We use it instead of blocking read calls to detect interrupts
  zmq::poll( requestsPoller, 4, -1 );
  
  if( requestsPoller[ 0 ].revents == ZMQ_POLLIN )
  {
    // We received a I/O Pub message
    zmq::message_t dummyMessage;
    std::cout << "Received IOPub message (weird...)" << std::endl;
    ioPubSocket.recv( &dummyMessage );
  }
  
  for( size_t socketIndex = 1; socketIndex < 4; socketIndex++ )
  {
    if( requestsPoller[ socketIndex ].revents == ZMQ_POLLIN )
    {
      JupyterMessage newMessage;
      
      if( !ReceiveProtocolMessage( socketsList[ socketIndex ], newMessage ) ) continue;
      
      // Enqueue new protocol message
      messageInQueue.push( newMessage );
    }
  }
}

void JupyterKernelConnection::SendMessage( JupyterMessage& message )
{      
  std::string messageType = message.GetTypeString();
  
  message.SetSessionUUID( sessionUUID );
  
  if( messageType.find( "_reply" ) != std::string::npos )
  {
    if( messageType == "shutdown_reply" or messageType == "connect_reply" )
      SendProtocolMessage( &controlSocket, message );
    else
      SendProtocolMessage( &shellSocket, message );
  }
  else if( messageType == "input_request" )
    SendProtocolMessage( &stdinSocket, message );
  else
    SendProtocolMessage( &ioPubSocket, message );
  
  return;
}

void JupyterKernelConnection::Shutdown()
{
  if( isRunning )
  {
    isRunning = false;
    heartbeatThread.join();   // Wait for the Heartbeat thread to return
    std::cout << "Heartbeat thread exited" << std::endl;
    
    ioPubSocket.close();
    controlSocket.close();
    stdinSocket.close();
    shellSocket.close();
  }
}


bool JupyterKernelConnection::ReceiveProtocolMessage( zmq::socket_t* receivingSocket, JupyterMessage& message )
{
  zmq::message_t messagePart;
  std::string serialHeader, serialParent, serialMetadata, serialContent;
  
  receivingSocket->recv( &messagePart );    // Uuid
  message.identifier.assign( (char*) messagePart.data() );
  
  receivingSocket->recv( &messagePart );    // Delimiter
  //if( strncmp( (char*) messagePart.data(), JupyterMessage::DELIMITER, messagePart.size() ) != 0 ) return false;
  
  receivingSocket->recv( &messagePart );    // HMAC signature
  
  receivingSocket->recv( &messagePart );    // Header
  serialHeader.assign( (char*) messagePart.data() );
  
  receivingSocket->recv( &messagePart );    // Parent Header
  serialParent.assign( (char*) messagePart.data() );
  
  receivingSocket->recv( &messagePart );    // Metadata
  serialMetadata.assign( (char*) messagePart.data() );
  
  receivingSocket->recv( &messagePart );    // Content
  serialContent.assign( (char*) messagePart.data() );
  
  message.DeserializeData( serialHeader, serialParent, serialMetadata, serialContent );
  
  std::cout << std::endl;
  
  return true;
}

void JupyterKernelConnection::SendProtocolMessage( zmq::socket_t* sendingSocket, JupyterMessage& message )
{
  zmq::message_t uuidMessage( message.identifier.data(), message.identifier.size() );
  sendingSocket->send( uuidMessage, ZMQ_SNDMORE );                // Uuid 
  
  zmq::message_t delimiterMessage( JupyterMessage::DELIMITER, strlen( JupyterMessage::DELIMITER ) );
  sendingSocket->send( delimiterMessage, ZMQ_SNDMORE );           // Delimiter
  
  message.UpdateTimeStamp();
  
  std::string serialHeader, serialParent, serialMetadata, serialContent;
  message.SerializeData( serialHeader, serialParent, serialMetadata, serialContent );
  
  std::string hmacSignature = messageHashGenerator.GenerateHash( serialHeader, serialParent, serialMetadata, serialContent );
  zmq::message_t hmacSignatureMessage( hmacSignature.data(), hmacSignature.size() );
  sendingSocket->send( hmacSignatureMessage, ZMQ_SNDMORE );       // HMAC signature
  
  zmq::message_t headerMessage( serialHeader.data(), serialHeader.size() );
  sendingSocket->send( headerMessage, ZMQ_SNDMORE );              // Header
  
  zmq::message_t parentMessage( serialParent.data(), serialParent.size() );
  sendingSocket->send( parentMessage, ZMQ_SNDMORE );              // Parent Header
  
  zmq::message_t metadataMessage( serialMetadata.data(), serialMetadata.size() );
  sendingSocket->send( metadataMessage, ZMQ_SNDMORE );            // Metadata
  
  zmq::message_t contentMessage( serialContent.data(), serialContent.size() );
  sendingSocket->send( contentMessage );                          // Content
}

void JupyterKernelConnection::RunHeartbeatLoop( std::string transport, std::string ipHost, std::string port, bool* ref_isRunning )
{
  // Creating Heartbeat socket on arbitrary port on its own thread
  zmq::socket_t heartbeatSocket( context, ZMQ_REP );
  heartbeatSocket.bind( transport + "://" + ipHost + ":" + port );
  
  *ref_isRunning = true;
  while( *ref_isRunning )
  {
    zmq::message_t ping; // We recreate message each time as "send" nullifies it
    
    heartbeatSocket.recv( &ping ); // Client asks if kernel is still running
    
    heartbeatSocket.send( ping ); // Answer immediately
  }
}
