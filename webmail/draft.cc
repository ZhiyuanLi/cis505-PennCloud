
//#include<iostream>
#include <stdio.h>
#include <string>
#include <fstream>
#include <iostream>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <regex>
#include <mutex>
#include <set>
#include <openssl/md5.h>
#include <time.h>
#include <cstddef>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <netdb.h>
//using namespace std;
#define EHLO "EHLO ***\r\n" //***为邮箱用户名
#define DATA "data\r\n"
#define QUIT "QUIT\r\n"

//#define h_addr h_addr_list[0]
//FILE *fin;
int sock;
struct sockaddr_in server;
struct hostent *hp, *gethostbyname();
char buf[BUFSIZ+1];
int len;
char *host_id="smtp.126.com";
char *from_id="atrueworld@126.com";
char *to_id="576161470@qq.com";
char *sub="testmail\r\n";
char wkstr[100]="hello how r u\r\n";

/*=====Send a string to the socket=====*/
void send_socket(char *s)
{
    write(sock,s,strlen(s));
    //write(1,s,strlen(s));
    //printf("Client:%s\n",s);
}

//=====Read a string from the socket=====*/
void read_socket()
{
    len = read(sock,buf,BUFSIZ);
    write(1,buf,len);
    //printf("Server:%s\n",buf);
}

/*=====MAIN=====*/
int main(int argc, char* argv[])
{

    /*=====Create Socket=====*/
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock==-1)
    {
        perror("opening stream socket");
        //exit(1);
        return 1;
    }
    else
        //cout << "socket created\n";
        printf("socket created\n");

    /*=====Verify host=====*/
    server.sin_family = AF_INET;
    hp = gethostbyname(host_id);
    if (hp==(struct hostent *) 0)
    {
        fprintf(stderr, "%s: unknown host\n", host_id);
        //exit(2);
        return 2;
    }

    /*=====Connect to port 25 on remote host=====*/
    memcpy((char *) &server.sin_addr, (char *) hp->h_addr, hp->h_length);
    server.sin_port=htons(25); /* SMTP PORT */
    if (connect(sock, (struct sockaddr *) &server, sizeof server)==-1)
    {
        perror("connecting stream socket");
        //exit(1);
        return 1;
    }
    else
        //cout << "Connected\n";
        printf("Connected\n");

    /*=====Write some data then read some =====*/
    read_socket(); /* SMTP Server logon string */
    send_socket(EHLO); /* introduce ourselves */
    read_socket(); /*Read reply */

    /*
    **added by fupeng
    */
    send_socket("AUTH LOGIN");
    send_socket("\r\n");
    read_socket();
    send_socket("YXRydWV3b3JsZA==");//用户名的base64编码
    send_socket("\r\n");
    read_socket();
    send_socket("cnl4Y3d5elhHITg=");//密码的base64编码
    send_socket("\r\n");
    read_socket();

    send_socket("mail from <");
    send_socket(from_id);
    send_socket(">");
    send_socket("\r\n");
    read_socket(); /* Sender OK */

    //send_socket("VRFY ");
    //send_socket(from_id);
    //send_socket("\r\n");
    //read_socket(); // Sender OK */
    send_socket("rcpt to <"); /*Mail to*/
    send_socket(to_id);
    send_socket(">");
    send_socket("\r\n");
    read_socket(); // Recipient OK*/

    send_socket(DATA);// body to follow*/
    read_socket();
    //send_socket("from:***@126.com");
    send_socket("subject:");
    send_socket(sub);
    //read_socket(); // Recipient OK*/
    send_socket("\r\n\r\n");
    send_socket(wkstr);
    send_socket(".\r\n");
    read_socket();
    send_socket(QUIT); /* quit */
    read_socket(); // log off */

    //=====Close socket and finish=====*/
    close(sock);
    //exit(0);
    return 0;
}




/* This is an example showing how to send mail using libcurl's SMTP
 * capabilities. It builds on the smtp-mail.c example to demonstrate how to use
 * libcurl's multi interface.
 *
 * Note that this example requires libcurl 7.20.0 or above.
 */

 /*

#define FROM     "<sender@example.com>"
#define TO       "<recipient@example.com>"
#define CC       "<info@example.com>"

#define MULTI_PERFORM_HANG_TIMEOUT 60 * 1000

static const char *payload_text[] = {
		"Date: Mon, 29 Nov 2010 21:54:29 +1100\r\n",
		"To: " TO "\r\n",
		"From: " FROM "(Example User)\r\n",
		"Cc: " CC "(Another example User)\r\n",
		"Message-ID: <dcd7cb36-11db-487a-9f3a-e652a9458efd@"
		"rfcpedant.example.org>\r\n",
		"Subject: SMTP multi example message\r\n",
		"\r\n", // empty line to divide headers from body, see RFC5322
		"The body of the message starts here.\r\n",
		"\r\n",
		"It could be a lot of lines, could be MIME encoded, whatever.\r\n",
		"Check RFC5322.\r\n",
		NULL
};

struct upload_status {
	int lines_read;
};

static size_t payload_source(void *ptr, size_t size, size_t nmemb, void *userp)
{
	struct upload_status *upload_ctx = (struct upload_status *)userp;
	const char *data;

	if((size == 0) || (nmemb == 0) || ((size*nmemb) < 1)) {
		return 0;
	}

	data = payload_text[upload_ctx->lines_read];

	if(data) {
		size_t len = strlen(data);
		memcpy(ptr, data, len);
		upload_ctx->lines_read++;

		return len;
	}

	return 0;
}

static struct timeval tvnow(void)
{
	struct timeval now;

	// time() returns the value of time in seconds since the epoch
	now.tv_sec = (long)time(NULL);
	now.tv_usec = 0;

	return now;
}

static long tvdiff(struct timeval newer, struct timeval older)
{
	return (newer.tv_sec - older.tv_sec) * 1000 +
			(newer.tv_usec - older.tv_usec) / 1000;
}
*/
//int main(void)
//{

/*
	CURL *curl;
	CURLM *mcurl;
	int still_running = 1;
	struct timeval mp_start;
	struct curl_slist *recipients = NULL;
	struct upload_status upload_ctx;

	upload_ctx.lines_read = 0;

	curl_global_init(CURL_GLOBAL_DEFAULT);

	curl = curl_easy_init();
	if(!curl)
		return 1;

	mcurl = curl_multi_init();
	if(!mcurl)
		return 2;

	// This is the URL for your mailserver
	curl_easy_setopt(curl, CURLOPT_URL, "smtp://mail.example.com");

	// Note that this option isn't strictly required, omitting it will result in
	 * libcurl sending the MAIL FROM command with empty sender data. All
	 * autoresponses should have an empty reverse-path, and should be directed
	 * to the address in the reverse-path which triggered them. Otherwise, they
	 * could cause an endless loop. See RFC 5321 Section 4.5.5 for more details.

	curl_easy_setopt(curl, CURLOPT_MAIL_FROM, FROM);

	// Add two recipients, in this particular case they correspond to the
	// To: and Cc: addressees in the header, but they could be any kind of
	// recipient.
	recipients = curl_slist_append(recipients, TO);
	recipients = curl_slist_append(recipients, CC);
	curl_easy_setopt(curl, CURLOPT_MAIL_RCPT, recipients);

	// We're using a callback function to specify the payload (the headers and
	// body of the message). You could just use the CURLOPT_READDATA option to
	// specify a FILE pointer to read from.
	curl_easy_setopt(curl, CURLOPT_READFUNCTION, payload_source);
	curl_easy_setopt(curl, CURLOPT_READDATA, &upload_ctx);
	curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L);

	// Tell the multi stack about our easy handle
	curl_multi_add_handle(mcurl, curl);

	// Record the start time which we can use later
	mp_start = tvnow();

	// We start some action by calling perform right away
	curl_multi_perform(mcurl, &still_running);

	while(still_running) {
		struct timeval timeout;
		fd_set fdread;
		fd_set fdwrite;
		fd_set fdexcep;
		int maxfd = -1;
		int rc;
		CURLMcode mc; // curl_multi_fdset() return code

		long curl_timeo = -1;

		// Initialise the file descriptors
		FD_ZERO(&fdread);
		FD_ZERO(&fdwrite);
		FD_ZERO(&fdexcep);

		// Set a suitable timeout to play around with
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		curl_multi_timeout(mcurl, &curl_timeo);
		if(curl_timeo >= 0) {
			timeout.tv_sec = curl_timeo / 1000;
			if(timeout.tv_sec > 1)
				timeout.tv_sec = 1;
			else
				timeout.tv_usec = (curl_timeo % 1000) * 1000;
		}

		// get file descriptors from the transfers
		mc = curl_multi_fdset(mcurl, &fdread, &fdwrite, &fdexcep, &maxfd);

		if(mc != CURLM_OK) {
			fprintf(stderr, "curl_multi_fdset() failed, code %d.\n", mc);
			break;
		}

		// On success the value of maxfd is guaranteed to be >= -1. We call
       select(maxfd + 1, ...); specially in case of (maxfd == -1) there are
       no fds ready yet so we call select(0, ...) --or Sleep() on Windows--
       to sleep 100ms, which is the minimum suggested value in the
       curl_multi_fdset() doc.

		if(maxfd == -1) {
#ifdef _WIN32
			Sleep(100);
			rc = 0;
#else
			// Portable sleep for platforms other than Windows.
			struct timeval wait = { 0, 100 * 1000 }; // 100ms
			rc = select(0, NULL, NULL, NULL, &wait);
#endif
		}
		else {
			// Note that on some platforms 'timeout' may be modified by select().
         If you need access to the original value save a copy beforehand.
			rc = select(maxfd+1, &fdread, &fdwrite, &fdexcep, &timeout);
		}

		if(tvdiff(tvnow(), mp_start) > MULTI_PERFORM_HANG_TIMEOUT) {
			fprintf(stderr,
					"ABORTING: Since it seems that we would have run forever.\n");
			break;
		}

		switch(rc) {
		case -1:  // select error
			break;
		case 0:   // timeout
		default:  // action
			curl_multi_perform(mcurl, &still_running);
			break;
		}
	}

	// Free the list of recipients
	curl_slist_free_all(recipients);

	// Always cleanup
	curl_multi_remove_handle(mcurl, curl);
	curl_multi_cleanup(mcurl);
	curl_easy_cleanup(curl);
	curl_global_cleanup();
*/
//	return 0;
//}






