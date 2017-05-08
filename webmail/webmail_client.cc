/*
 * webmail_client.cc
 *
 *  Created on: Apr 22, 2017
 *      Author: cis505
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

#include "webmail_utils.h"

int main(int argc, char *argv[])
{

  // Initialize the buffers

  struct connection conn1;
  initializeBuffers(&conn1, 5000);

  // Open a connection and exchange greeting messages

  connectToPort(&conn1, 2300);
  DoRead(&conn1);
  expectNoMoreData(&conn1);

  writeString(&conn1, "Send\r\nFrom: Benjamin Franklin <benjamin.franklin@localhost.com>\r\nTo: Mengjin <mengjin@seas.upenn.edu>\r\nDate: Fri, 21 Oct 2016 18:29:11 -0400\r\nSubject: Testing my new email account\r\nMengjin,\r\nI just wanted to see whether my new email account works.\r\n            - Ben\r\n.\r\n");

  DoRead(&conn1);
  expectNoMoreData(&conn1);

//  writeString(&conn1, "HELO tester\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  // Specify the sender and the receipient (with one incorrect recipient)
//
//  writeString(&conn1, "MAIL FROM:<benjamin.franklin@localhost.com>\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  writeString(&conn1, "RCPT TO:<mengjin@seas.upenn.edu>\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  writeString(&conn1, "RCPT TO:<nonexistent.mailbox@localhost.com>\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  writeString(&conn1, "RCPT TO:<linhphan@localhost.com>\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  // Send the actual data
//
//  writeString(&conn1, "DATA\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);
//
//  writeString(&conn1, "From: Benjamin Franklin <benjamin.franklin@localhost>\r\n");
//  writeString(&conn1, "To: Mengjin <mengjin@seas.upenn.edu>\r\n");
//  writeString(&conn1, "Date: Fri, 21 Oct 2016 18:29:11 -0400\r\n");
//  writeString(&conn1, "Subject: Testing my new email account\r\n");
//  writeString(&conn1, "\r\n");
//  writeString(&conn1, "Mengjin,\r\n");
//  writeString(&conn1, "\r\n");
//  writeString(&conn1, "I just wanted to see whether my new email account works.\r\n");
//  writeString(&conn1, "\r\n");
//  writeString(&conn1, "        - Ben\r\n");
//  expectNoMoreData(&conn1);
//  writeString(&conn1, ".\r\n");
//  DoRead(&conn1);
//  expectNoMoreData(&conn1);

  // Close the connection

  writeString(&conn1, "QUIT\r\n");
  DoRead(&conn1);
  expectRemoteClose(&conn1);
  closeConnection(&conn1);

  freeBuffers(&conn1);
  return 0;
}
