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

/**
 ** \file JupyterMessage.hxx
 ** \brief Define class JupyterKernelConnection.
 */

#ifndef JUPYTER_KERNEL_CONNECTION_HXX
#define JUPYTER_KERNEL_CONNECTION_HXX

#include <zmq.hpp>      // ZeroMQ functions
#include "JupyterMessage.hxx"
#include "JupyterMessageHash.hxx"
#include <string>
#include <queue>
#include <string>
#include <thread>       // C++11 threads

/**
 ** \brief Define class JupyterMessage.
 **
 ** Encapsulates Jupyter protocol multichannel [ZeroMQ] connection.
 ** Abstracts out routing of messages to each channel, resolving it
 ** internally according to the message type.
 */
class JupyterKernelConnection
{
public:
  /** \brief Server-sider Jupyter protocol connection constructor (starts update threads).
  ** \param transport transport protocol identifier (like "tcp" or "ipc").
  ** \param ipHost connection local address host (like "localhost" or "127.0.0.1").
  ** \param ioPubPort I/O publisher channel local port.
  ** \param controlPort control channel local port.
  ** \param stdinPort stdin channel local port.
  ** \param shellPort shell channel local port.
  ** \param heartbeatPort heartbeat channel local port. */
  JupyterKernelConnection( std::string transport, std::string ipHost, std::string ioPubPort, 
                           std::string controlPort, std::string stdinPort, std::string shellPort, std::string heartbeatPort ); 
  /** \brief Connection destructor. */
  ~JupyterKernelConnection();
  
  /** \brief Define internal message hash generator parameters.
  ** \param keyString HMAC string key.
  ** \param signatureScheme hash algorithm string identifier. */
  void SetHashGenerator( std::string keyString, std::string signatureScheme );
  /** \brief Receive client messages and fill input queue in priority order.
  ** \param messageInQueue input messages queue storage. */
  void ReceiveMessages( std::queue<JupyterMessage>& messageInQueue );
  /** \brief Send single message to one (reply/request) or many (publish) clients.
  ** \param message Jupyter multipart message to be sent. */
  void SendMessage( JupyterMessage& message );
  /** \brief Close connection and stop update threads. */
  void Shutdown();
  
private:
  /** ZeroMQ connection context (shared across entire application). */
  static zmq::context_t context;
  /** Connection session unique string identifier. */
  std::string sessionUUID;
  /** ZeroMQ connection sockets handled on main thread. */
  zmq::socket_t ioPubSocket, controlSocket, stdinSocket, shellSocket;
  /** Internal message HMAC string (hash) generator. */
  MessageHashGenerator messageHashGenerator;
  /** Define if heartbeat thread should keep running or not. */
  bool isRunning;
  
  /** ZeroMQ poller for detecting incoming Shell/Control request and Stdin reply messages. */
  zmq::pollitem_t requestsPoller[ 4 ];
  
  /** \brief Internal message receiving helper function.
  ** \param receivingSocket ZeroMQ socket where message is received.
  ** \param message input Jupyter message storage. 
  ** \result true if message receiving succeeded */
  bool ReceiveProtocolMessage( zmq::socket_t* receivingSocket, JupyterMessage& message );
  /** \brief Internal message sending helper function.
  ** \param sendingSocket ZeroMQ socket from where message is sent.
  ** \param message output Jupyter message. */
  void SendProtocolMessage( zmq::socket_t* sendingSocket, JupyterMessage& message );
  
  /** Heartbeat handling thread handle. */
  std::thread heartbeatThread;
  /** \brief Heartbeat handling thread function.
  ** \param transport transport protocol identifier (like "tcp" or "ipc").
  ** \param ipHost connection local address host (like "localhost" or "127.0.0.1").
  ** \param port heartbeat channel local port.
  ** \param ref_isRunning running signaler flag pointer. */
  static void RunHeartbeatLoop( std::string transport, std::string ipHost, std::string port, bool* ref_isRunning );
};

#endif // JUPYTER_KERNEL_CONNECTION_HXX
