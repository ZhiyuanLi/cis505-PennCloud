#include <arpa/inet.h>
#include <dirent.h>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <vector>

#include "../webmail/webmail_utils.h"
#include "admin_console.h"
#include "constants.h"
#include "request.h"
#include "response.h"
#include "store.h"
#include "utils.h"

using namespace std;

// username
string user_name;
char *curr_user;

Response::Response(Request req) {
  this->http_version = req.http_version;
  (this->headers)[CONNECTION] = "close";

  const char *filename = ("." + req.path).c_str();

  // static file
  if (req.path != HOME_URL && is_file_exist(filename)) {
    file(filename);
  }

  // admin console
  else if (req.path == ADMIN_URL) {
    admin_console(req);
  }

  // register
  else if (req.path == REGISTER_URL) {
    reg(req);
  }

  // login
  else if (req.path == LOGIN_URL) {
    login(req);
  }

  // user not login
  else if (!is_already_login(req.cookies, user_name)) {
    login(req);
  }

  // upload
  else if (req.path == UPLOAD_URL) {
    if (req.method == "GET") {
      upload(req);
    } else if (req.method == "POST") {
      handle_upload(req);
    }
  }

  // download file
  else if (req.path.length() >= 10 && req.path.substr(0, 10) == "/download?") {
    download_file(req);
  }

  // download
  else if (req.path.length() >= 9 && req.path.substr(0, 9) == DOWNLOAD_URL) {
    download(req);
  }

  // create new folder
  else if (req.path == NEW_FOLDER_URL) {
    create_new_folder(req);
  }

  // rename file
  else if (req.path == RENAME_FILE_URL) {
    rename_file(req);
  }

  // delete file
  else if (req.path == DELETE_FILE_URL) {
    delete_file(req);
  }

  // delete folder
  else if (req.path == DELETE_FOLDER_URL) {
    delete_folder(req);
  }

  // move file
  else if (req.path == MOVE_FILE_URL) {
    move_file(req);
  }

  // send email
  else if (req.path == SEND_EMAIL_URL) {
    if (req.method == "GET") {
      send_email(req);
    } else if (req.method == "POST") {
      handle_send_email(req);
    }
  }

  // inbox
  else if (req.path == INBOX_URL) {
    inbox(req);
  }

  // view email
  else if (req.path.length() >= 10 &&
           req.path.substr(0, 10) == VIEW_EMAIL_URL) {
    view_email(req);
  }

  // forward email
  else if (req.path.length() >= 8 &&
           req.path.substr(0, 8) == FORWARD_EMAIL_URL) {
    forward_email(req);
  }

  // reply email
  else if (req.path.length() >= 11 &&
           req.path.substr(0, 11) == REPLY_EMAIL_URL) {
    reply_email(req);
  }

  // delete email
  else if (req.path.length() >= 12 &&
           req.path.substr(0, 12) == DELETE_EMAIL_URL) {
    delete_email(req);
  }

  // logout
  else if (req.path == LOGOUT_URL) {
    delete_session(user_name);
    login(req);
  }

  // otherwise login
  else {
    login(req);
  }
}

void Response::reply(int fd) {
  string rep;
  rep += http_version + " " + status + "\r\n";

  for (map<string, string>::iterator it = headers.begin(); it != headers.end();
       ++it) {
    rep += it->first + it->second + "\r\n";
  }
  rep += "\r\n";
  // rep += this->body;

  char crep[rep.length() + 1];
  strcpy(crep, rep.c_str());

  // send header
  debug(1, "[%d] S: %s", fd, crep);
  do_write(fd, crep, strlen(crep));

  // send body
  for (int i = 0; i < (this->body).length(); i++) {
    do_write(fd, &(this->body).at(i), 1);
  }
}

/* register */
void Response::reg(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";

  // GET
  if (req.method.compare("GET") == 0) {
    this->body = get_file_content_as_string("html/register.html");
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  // POST
  else if (req.method.compare("POST") == 0) {
    vector<string> params = split(req.body.c_str(), '&');
    string username = split(params.at(0), '=').at(1);
    string password = split(params.at(1), '=').at(1);

    DIR *dir = opendir(username.c_str());
    if (dir) {
      /* Directory exists. */
      closedir(dir);
    } else if (ENOENT == errno) {
      /* Directory does not exist. */
      mkdir(username.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    }
    curr_user = (char *)(username + "/").c_str();

    if (is_user_exist(username)) {
      this->body = get_file_content_as_string("html/user-already-exist.html");
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    } else { // new user
      add_user(username, password);
      this->body = get_file_content_as_string("html/new-user.html");
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    }
  }
}

/* login */
void Response::login(Request req) {
  string username;
  bool already_login = is_already_login(req.cookies, username);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";

  // GET
  if (req.method.compare("GET") == 0 && !already_login) {
    this->body = get_file_content_as_string("html/login.html");
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  // POST
  else if (req.method.compare("POST") == 0) {
    vector<string> params = split(req.body.c_str(), '&');
    username = split(params.at(0), '=').at(1);
    string password = split(params.at(1), '=').at(1);

    if (is_login_valid(username, password)) {
      add_session(username);
      already_login = true;
    } else {
      this->body = get_file_content_as_string("html/login-failed.html");
      (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    }
  }

  if (already_login) {
    this->body = get_file_content_as_string("html/user-home.html");
    replace_all(this->body, "$username", username);
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
    (this->headers)[SET_COOKIE] = "sessionid=" + username;
  }
}

/*tell is login or not from sessions*/
bool Response::is_already_login(map<string, string> cookies, string &username) {
  if (cookies.count("sessionid") == 1) {
    if (is_session_valid(cookies["sessionid"])) {
      username = cookies["sessionid"];
      // curr_user = (char *)(username + "/").c_str();
      return true;
    }
  }
  return false;
}

/****************
 * file storage *
 ****************/

/* file upload */
void Response::upload(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/upload.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* handle file upload */
void Response::handle_upload(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string dir(curr_user);
  string filename(extract_file_name(req.body));
  string file_content(extract_file_content(req.body, req.content_length));
  store_file(dir, filename, file_content);

  // send to KV store
  string message("put " + user_name + "," + filename + "," + file_content +
                 "\r\n");
  send_to_backend(message, user_name);

  this->body = get_file_content_as_string("html/upload.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* download all uploaded files */
void Response::download(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string dir(curr_user);
  string curr_folder(req.path.substr(9));

  // send to KV store
  string message("getlist " + user_name + ",file" + "\r\n");
  vector<string> lines = send_to_backend(message, user_name);

  vector<string> files(list_all_files(dir));
  stringstream ss_file;
  stringstream ss_folder;

  if (curr_folder.length() > 0) {
    // remove "/"
    curr_folder = curr_folder.substr(1);
    for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
      string file_path(dir + *it);
      string saved_filename(*it);
      if ((*it).find(curr_folder + "+") != string::npos) {
        int slash = (*it).find("+");
        saved_filename = saved_filename.substr(slash + 1);
        if (*it == curr_folder + "+" + saved_filename) {
          ss_file << "<a href=\"?" << file_path;
          ss_file << "\" class=\"list-group-item\" >";
          // ss_file << "\" class=\"list-group-item\" download=\"";
          // ss_file << saved_filename << "\">";
          ss_file << saved_filename;
          ss_file << "</a>";
        }
      }
    }

    this->body = get_file_content_as_string("html/download-subfolder.html");
    string all_download_files = ss_file.str();
    replace_all(this->body, "$allDownloadFiles", all_download_files);
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }

  else {
    for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
      string file_path(dir + *it);
      string saved_filename(*it);
      string folder_path(*it);
      if ((*it).find("+") == string::npos) {
        if (file_path.find(".folder") == string::npos) {
          ss_file << "<a href=\"?" << file_path;
          ss_file << "\" class=\"list-group-item\" >";
          // ss_file << "\" class=\"list-group-item\" download=\"";
          // ss_file << saved_filename << "\">";
          ss_file << *it;
          ss_file << "</a>";
        } else {
          folder_path = folder_path.substr(0, folder_path.length() - 7);
          ss_folder << "<a href=\"download/" << folder_path;
          ss_folder << "\" class=\"list-group-item\" >";
          ss_folder << folder_path;
          ss_folder << "</a>";
        }
      }
    }

    this->body = get_file_content_as_string("html/download.html");
    string all_download_files = ss_file.str();
    string all_folders = ss_folder.str();
    replace_all(this->body, "$allDownloadFiles", all_download_files);
    replace_all(this->body, "$allFolders", all_folders);
    (this->headers)[CONTENT_LEN] = to_string((this->body).length());
  }
}

/* display file content */
void Response::file(const char *filename) {
  debug(1, "[file exists]: ");
  debug(1, filename);
  debug(1, "\r\n");
  string file_content(get_file_content_as_string(filename));
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/plain";
  this->body = file_content;
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* create new folder */
void Response::create_new_folder(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string foldername = req.body.substr(req.body.find('=') + 1);
  string dir(curr_user);
  foldername += ".folder";
  store_file(dir, foldername, "");

  // send to KV store
  // string message("put " + user_name + "," + foldername + "," + "empty" +
  //                "\r\n");
  // send_to_backend(message, user_name);

  this->body =
      get_file_content_as_string("html/create-new-folder-success.html");
  replace_all(this->body, "$foldername", foldername);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* rename file */
void Response::rename_file(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  vector<string> tokens = split(req.body.c_str(), '&');
  string foldername = tokens.at(0).substr(tokens.at(0).find('=') + 1);
  string oldname = tokens.at(1).substr(tokens.at(1).find('=') + 1);
  string newname = tokens.at(2).substr(tokens.at(2).find('=') + 1);
  if (foldername.length() > 0) {
    // add "+"
    foldername += "+";
  }

  string dir(curr_user);
  rename((dir + foldername + oldname).c_str(),
         (dir + foldername + newname).c_str());
  this->body = get_file_content_as_string("html/rename-file-success.html");
  replace_all(this->body, "$oldname", oldname);
  replace_all(this->body, "$newname", newname);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* delete file */
void Response::delete_file(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  vector<string> tokens = split(req.body.c_str(), '&');
  string foldername = tokens.at(0).substr(tokens.at(0).find('=') + 1);
  string filename = tokens.at(1).substr(tokens.at(1).find('=') + 1);
  if (foldername.length() > 0) {
    // add "+"
    foldername += "+";
  }

  string dir(curr_user);
  remove((dir + foldername + filename).c_str());

  // send to KV store
  string message("dele " + user_name + "," + foldername + filename + "\r\n");
  send_to_backend(message, user_name);

  this->body = get_file_content_as_string("html/delete-file-success.html");
  replace_all(this->body, "$filename", filename);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* delete folder */
void Response::delete_folder(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string foldername = req.body.substr(req.body.find('=') + 1);
  string dir(curr_user);
  vector<string> files(list_all_files(dir));

  for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
    string file_path(dir + *it);
    if (foldername.length() > 0) {
      if (*it == (foldername + ".folder") ||
          (*it).find(foldername + "+") != string::npos) {
        remove(file_path.c_str());

        // send to KV store
        // string message("dele " + user_name + "," + *it + "\r\n");
        // send_to_backend(message, user_name);
      }
    } else {
      remove(file_path.c_str());

      // send to KV store
      // string message("dele " + user_name + "," + *it + "\r\n");
      // send_to_backend(message, user_name);
    }
  }

  this->body = get_file_content_as_string("html/delete-folder-success.html");
  replace_all(this->body, "$foldername", foldername);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* move file */
void Response::move_file(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  vector<string> tokens = split(req.body.c_str(), '&');
  string filename = tokens.at(0).substr(tokens.at(0).find('=') + 1);
  string oldfolder = tokens.at(1).substr(tokens.at(1).find('=') + 1);
  string newfolder = tokens.at(2).substr(tokens.at(2).find('=') + 1);

  string dir(curr_user);
  if (oldfolder.length() > 0) {
    rename((dir + oldfolder + "+" + filename).c_str(),
           (dir + newfolder + "+" + filename).c_str());
  } else {
    rename((dir + filename).c_str(),
           (dir + newfolder + "+" + filename).c_str());
  }

  this->body = get_file_content_as_string("html/move-file-success.html");
  replace_all(this->body, "$filename", filename);
  replace_all(this->body, "$newfolder", newfolder);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* download file */
void Response::download_file(Request req) {
  string file_path = req.path.substr(10);
  int filesize = file_size(file_path.c_str());
  this->status = OK;
  (this->headers)[CONTENT_LEN] = to_string(filesize);

  // check content type
  if (file_path.find("jpeg") != string::npos ||
      file_path.find("jpg") != string::npos) {
    (this->headers)[CONTENT_TYPE] = "image/jpeg";
  } else if (file_path.find("png") != string::npos) {
    (this->headers)[CONTENT_TYPE] = "image/png";
  } else if (file_path.find("pdf") != string::npos) {
    (this->headers)[CONTENT_TYPE] = "application/pdf";
  } else {
    (this->headers)[CONTENT_TYPE] = "text/plain";
  }

  // get file content
  char *buf = (char *)malloc(sizeof(char) * filesize);
  fstream file(file_path.c_str(), ios::in | ios::out | ios::binary);
  this->body = "";
  for (int i = 0; i < filesize; i++) {
    file.read(buf + i, 1);
    this->body += buf[i];
  }
  free(buf);
}

/*********
 * email *
 *********/

/* send email */
void Response::send_email(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/send-email.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* handle send email */
void Response::handle_send_email(Request req) {
  vector<string> params = split(url_decode(req.body).c_str(), '&');
  string sent_to = split(params.at(0), '=').at(1);
  string title = split(params.at(1), '=').at(1);
  string content = split(params.at(2), '=').at(1);
  string curr_time = get_current_time();

  // decoding
  // replace_all(sent_to, "%40", "@");
  replace_all(title, "+", " ");
  replace_all(content, "+", " ");

  string message;
  message += "Send\r\n";
  message += "From: <" + user_name + "@localhost.com>\r\n";
  message += "To: <" + sent_to + ">\r\n";
  message += "Date: " + curr_time + "\r\n";
  message += "Subject: " + title + "\r\n";
  message += content + "\r\n";
  message += ".\r\n";
  send_to_email_server(message);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/send-email-success.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* inbox */
void Response::inbox(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/inbox.html");
  // send to KV store
  string message("getlist " + user_name + ",email" + "\r\n");
  vector<string> rep = send_to_backend(message, user_name);

  int count = 0;
  string maillist;
  debug(1, "Email List:\n");
  string content;
  for (int i = 0; i < rep.size(); i++) {

    string line = rep.at(i);

    // if (line.length() <= 16) {
    //   continue;
    // }
    if (line.at(0) == ',') {
      line = line.substr(1, line.length() - 1);
    }
    if (line.substr(0, 2).compare("##") == 0) { // get one new email
      count++;
      // FROM
      // cout << "FROM   " << line << endl;
      vector<string> f_tokens = split(line.c_str(), ',');
      string address = f_tokens.at(1).substr(7, f_tokens.at(1).length() - 8);

      // cout << address << '\n';

      maillist += "<tr>\n";
      maillist += "<th scope=\" row \">";
      maillist += "<a href=\"viewemail?";
      // maillist += "from=" + address;
      // maillist += "&title=" + rep.at(i + 3).substr(9);
      // maillist += "&date=" + rep.at(i + 2).substr(6);

      // content for each mail
      content.clear();
      int j = i + 4;
      while (!(rep.at(j).compare(".") == 0 && rep.at(j + 1).empty())) {
        content += rep.at(j) + "\r\n";
        j++;
      }
      j = j + 1;

      // maillist += "&content=" + content;
      maillist += "key=" + f_tokens.at(0).substr(2);

      // cout<<content<<'\n';

      maillist += "\">";
      maillist += to_string(count) + "</a></th>\n";
      maillist += "<td>" + address + "</td>\n";

      // Subject
      line = rep.at(i + 3);
      // cout << "Subject   " << line << endl;
      maillist += "<td>" + line.substr(9) + "</td>\n";

      // Date
      line = rep.at(i + 2);
      // cout << "Date   " << line << endl;
      maillist += "<td>" + line.substr(6) + "</td>\n";

      maillist += "</tr>\n";
      i = j;
    }
  }

  debug(1, "============End of Email List:\n");
  replace_all(this->body, "$maillist", maillist);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* view email */
void Response::view_email(Request req) {
  string path = req.path.substr(11);
  // vector<string> params = split(url_decode(path).c_str(), '&');
  // string from = split(params.at(0), '=').at(1);
  // string title = split(params.at(1), '=').at(1);
  // string date = split(params.at(2), '=').at(1);
  // string content = split(params.at(3), '=').at(1);
  string key = split(path, '=').at(1);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/view-email.html");

  // send to KV store
  string message("get " + user_name + ",##" + key + "\r\n");
  vector<string> rep = send_to_backend(message, user_name);

  int i = 1;

  string from = rep.at(i).substr(7, rep.at(i).length() - 8);
  string date = rep.at(i + 2).substr(6);
  string title = rep.at(i + 3).substr(9);

  // content for each mail
  string content;
  int j = i + 4;
  while (!(rep.at(j).compare(".") == 0 && rep.at(j + 1).empty())) {
    content += rep.at(j) + "\r\n";
    j++;
  }

  replace_all(this->body, "$from", from);
  replace_all(this->body, "$title", title);
  replace_all(this->body, "$date", date);
  replace_all(this->body, "$content", content);

  string query = "key=" + key;
  // reply
  replace_all(this->body, "$replyQuery", query);

  // forward
  replace_all(this->body, "$forwardQuery", query);

  // delete
  replace_all(this->body, "$deleteQuery", query);

  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* send message to email server */
void Response::send_to_email_server(string message) {
  // Initialize the buffers
  struct connection conn;
  initializeBuffers(&conn, 5000);

  // Open a connection and send messages
  connectToPort(&conn, 2300);
  writeString(&conn, message.c_str());

  // Close the connection
  writeString(&conn, "QUIT\r\n");
  DoRead(&conn);
  expectRemoteClose(&conn);
  closeConnection(&conn);
  freeBuffers(&conn);
}

/* forward email */
void Response::forward_email(Request req) {
  string path = req.path.substr(9);
  // vector<string> params = split(url_decode(path).c_str(), '&');
  string key = split(path, '=').at(1);

  // send to KV store
  string message("get " + user_name + ",##" + key + "\r\n");
  vector<string> rep = send_to_backend(message, user_name);

  int i = 1;

  string email = rep.at(i).substr(7, rep.at(i).length() - 8);
  string date = rep.at(i + 2).substr(6);
  string title = rep.at(i + 3).substr(9);

  // content for each mail
  string content;
  int j = i + 4;
  while (!(rep.at(j).compare(".") == 0 && rep.at(j + 1).empty())) {
    content += rep.at(j) + "\r\n";
    j++;
  }

  string title2 = "FW: " + title;
  string content2 = "\n==========\n||From: " + email + "\n" + "||Date: " +
                    date + "\n\n" + content;

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/forward-email.html");
  replace_all(this->body, "$title", title2);
  replace_all(this->body, "$content", content2);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* reply email */
void Response::reply_email(Request req) {
  string path = req.path.substr(12);
  string key = split(path, '=').at(1);
  // vector<string> params = split(url_decode(path).c_str(), '&');
  // string email = split(params.at(0), '=').at(1);
  // string title = "RE: " + split(params.at(1), '=').at(1);
  // string content = "==========\n\n" + split(params.at(2), '=').at(1);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/reply-email.html");
  // send to KV store
  string message("get " + user_name + ",##" + key + "\r\n");
  vector<string> rep = send_to_backend(message, user_name);

  int i = 1;

  string email = rep.at(i).substr(7, rep.at(i).length() - 8);
  string date = rep.at(i + 2).substr(6);
  string title = rep.at(i + 3).substr(9);

  // content for each mail
  string content;
  int j = i + 4;
  while (!(rep.at(j).compare(".") == 0 && rep.at(j + 1).empty())) {
    content += rep.at(j) + "\r\n";
    j++;
  }

  string title2 = "RE: " + title;
  string content2 = "\n==========\n||From: " + email + "\n" + "||Date: " +
                    date + "\n\n" + content;

  replace_all(this->body, "$email", email);
  replace_all(this->body, "$title", title2);
  replace_all(this->body, "$content", content2);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* delete email */
void Response::delete_email(Request req) {
  string path = req.path.substr(13);
  vector<string> params = split(url_decode(path).c_str(), '&');
  string key = split(params.at(0), '=').at(1);

  // send to KV store
  string message("dele " + user_name + ",##" + key + "\r\n");
  send_to_backend(message, user_name);

  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  this->body = get_file_content_as_string("html/delete-email.html");
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/*****************
 * admin console *
 *****************/

/* admin console */
void Response::admin_console(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";

  string frontend;
  string backend;
  int server_index = 0;

  parse_frontend_servers("servers.txt");

  for (int i = 1; i <= frontend_servers.size(); i++) {

    // if (server_list[server_index].servertype.compare("frontend") == 0) {

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    inet_aton(frontend_servers[i].ip.c_str(), &(servaddr.sin_addr));
    servaddr.sin_port = htons(frontend_servers[i].port);
    servaddr.sin_family = AF_INET;

    frontend += "<tr>";
    string frontend_ip =
        frontend_servers[i].ip + ":" + to_string(frontend_servers[i].port);
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
      pthread_mutex_lock(&mutex_lock);
      frontend_servers[i].running = true;
      pthread_mutex_unlock(&mutex_lock);

      cout << "frontend_server #" << i << " is active" << endl;

      frontend += "<td class=\"success\">" + frontend_ip + "</td>";
      frontend += "<td class=\"success\">Active</td>";
    } else {
      pthread_mutex_lock(&mutex_lock);
      frontend_servers[i].running = false;
      pthread_mutex_unlock(&mutex_lock);

      cout << "frontend_server #" << i << " is down" << endl;

      frontend += "<td class=\"danger\">" + frontend_ip + "</td>";
      frontend += "<td class=\"danger\">Down</td>";
    }

    frontend += "</tr>";
    close(sockfd);
  }

  int udp_fd = socket(PF_INET, SOCK_DGRAM, 0);
  struct sockaddr_in dest;
  bzero(&dest, sizeof(dest));
  dest.sin_family = AF_INET;
  dest.sin_port = htons(MASTER_PORT);
  inet_pton(AF_INET, MASTER_IP, &(dest.sin_addr));

  //	ask master for backend's ip:port
  string contact_master = "A";
  sendto(udp_fd, contact_master.c_str(), contact_master.size(), 0,
         (struct sockaddr *)&dest, sizeof(dest));
  cout << "To master: " << contact_master << endl;

  struct sockaddr_in src;
  socklen_t srcSize = sizeof(src);
  char feedback[1024];
  int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0,
                      (struct sockaddr *)&src, &srcSize);
  feedback[rlen] = 0;
  cout << "From master: " << feedback << endl;

  parse_backend_servers(feedback);

  for (int i = 1; i <= backend_servers.size(); i++) {

    int sockfd = socket(PF_INET, SOCK_STREAM, 0);
    struct sockaddr_in servaddr;
    inet_aton(backend_servers[i].ip.c_str(), &(servaddr.sin_addr));
    servaddr.sin_port = htons(backend_servers[i].port);
    servaddr.sin_family = AF_INET;

    backend += "<tr>";
    string backend_ip =
        backend_servers[i].ip + ":" + to_string(backend_servers[i].port);
    if (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0) {
      pthread_mutex_lock(&mutex_lock);
      backend_servers[i].running = true;
      pthread_mutex_unlock(&mutex_lock);

      cout << "backend_server #" << i << " is active" << endl;

      backend += "<td class=\"success\">" + backend_ip + "</td>";
      backend += "<td class=\"success\">Active</td>";
      string contact_master = "U" + backend_servers[i].ip + ":" +
                              to_string(backend_servers[i].port);
      sendto(udp_fd, contact_master.c_str(), contact_master.size(), 0,
             (struct sockaddr *)&dest, sizeof(dest));
      cout << "asking master username of node " << contact_master << endl;

      struct sockaddr_in src;
      socklen_t srcSize = sizeof(src);
      char feedback[5000];
      int rlen = recvfrom(udp_fd, feedback, sizeof(feedback) - 1, 0,
                          (struct sockaddr *)&src, &srcSize);

      feedback[rlen] = 0;
      cout << "user name list in this node: " << feedback << endl;

      string f(feedback);

      vector<string> tokens = split(f.c_str(), ',');
      for (auto name : tokens) {
        if (name.empty())
          continue;
        string message = "getlist " + name + ",email\r\n";
        cout << name << " emails:" << endl;
        backend += "<tr><td>";
        backend += name + " emails:</td>";
        vector<string> email_rep = send_to_backend(message, name);
        for (auto s : email_rep) {
          backend += "<td>";
          cout << s << endl;
          backend += s + "</td>";
        }
        backend += "</tr>";

        message = "getfile " + name + "\r\n";
        cout << name << " files:" << endl;
        backend += "<tr><td>";
        backend += name + " files and pwd:</td>";
        vector<string> file_rep = send_to_backend(message, name);
        for (auto s : file_rep) {
          backend += "<td>";
          cout << s << endl;
          backend += s + "</td>";
        }
        backend += "</tr>";
      }

    } else {
      pthread_mutex_lock(&mutex_lock);
      backend_servers[i].running = false;
      pthread_mutex_unlock(&mutex_lock);

      cout << "backend_server #" << i << " is down" << endl;

      backend += "<td class=\"danger\">" + backend_ip + "</td>";
      backend += "<td class=\"danger\">Down</td>";
    }

    backend += "</tr>";
    close(sockfd);
  }

  this->body = get_file_content_as_string("html/admin-console.html");
  replace_all(this->body, "$frontend", frontend);
  replace_all(this->body, "$backend", backend);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}
