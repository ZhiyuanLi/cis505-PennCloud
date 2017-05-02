#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

#include "constants.h"
#include "utils.h"
#include "request.h"
#include "response.h"
#include "store.h"

using namespace std;

Response::Response(Request req) {
  this->http_version = req.http_version;
  (this->headers)[CONNECTION] = "close";

  // static file
  const char* filename = ("." + req.path).c_str();
  if (is_file_exist(filename)) {
    file(filename);
  }

  // register
  else if (req.path == REGISTER_URL) {
    reg(req);
  }

  // login
  else if (req.path == LOGIN_URL) {
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
}

void Response::reply(int fd) {
  string rep;
  rep += http_version + " " + status + "\r\n";

  for (map<string, string>::iterator it = headers.begin(); it != headers.end();
       ++it) {
    rep += it->first + it->second + "\r\n";
  }
  rep += "\r\n";
  rep += body;

  char crep[rep.length() + 1];
  strcpy(crep, rep.c_str());

  debug(1, "[%d] S: %s", fd, crep);
  do_write(fd, crep, strlen(crep));
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
    vector<string> parameter_tokens = split(req.body.c_str(), '&');
    string username = split(parameter_tokens.at(0), '=').at(1);
    string password = split(parameter_tokens.at(1), '=').at(1);

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
    vector<string> parameter_tokens = split(req.body.c_str(), '&');
    username = split(parameter_tokens.at(0), '=').at(1);
    string password = split(parameter_tokens.at(1), '=').at(1);

    if (is_login_valid(username, password)) {
      add_session(username);
      already_login = true;
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
      return true;
    }
  }
  return false;
}

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
  (this->headers)[CONTENT_TYPE] = "text/plain";
  // (this->headers)[CONTENT_TYPE] = "application/pdf";
  string dir(UPLOADED_DIR);
  string filename(extract_file_name(req.body));
  this->body = extract_file_content(req.body, req.content_length);
  // this->body = get_file_content_as_string("html/redirect-to-download.html");
  store_file(dir, filename, this->body);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* download all uploaded files */
void Response::download(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string dir(UPLOADED_DIR);
  string curr_folder(req.path.substr(9));
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
          ss_file << "<a href=\"" << file_path;
          ss_file << "\" class=\"list-group-item\" download=\"";
          ss_file << saved_filename << "\">";
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
          ss_file << "<a href=\"" << file_path;
          ss_file << "\" class=\"list-group-item\" download=\"";
          ss_file << saved_filename << "\">";
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
void Response::file(const char* filename) {
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
  string dir(UPLOADED_DIR);
  store_file(dir, foldername + ".folder", "");
  this->body = get_file_content_as_string("html/create-new-folder-success.html");
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

  string dir(UPLOADED_DIR);
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

  string dir(UPLOADED_DIR);
  remove((dir + foldername + filename).c_str());
  this->body = get_file_content_as_string("html/delete-file-success.html");
  replace_all(this->body, "$filename", filename);
  (this->headers)[CONTENT_LEN] = to_string((this->body).length());
}

/* delete folder */
void Response::delete_folder(Request req) {
  this->status = OK;
  (this->headers)[CONTENT_TYPE] = "text/html";
  string foldername = req.body.substr(req.body.find('=') + 1);
  string dir(UPLOADED_DIR);
  vector<string> files(list_all_files(dir));

  for (vector<string>::iterator it = files.begin(); it != files.end(); ++it) {
    string file_path(dir + *it);
    if (foldername.length() > 0) {
      if (*it == (foldername + ".folder")
          || (*it).find(foldername + "+") != string::npos) {
        remove(file_path.c_str());
      }
    } else {
      remove(file_path.c_str());
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

  string dir(UPLOADED_DIR);
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
