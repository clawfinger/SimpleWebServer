#include <unistd.h>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <vector>
#include "http_parser.h"
#include <string.h>
#include <sstream>
#include <utility>
#include <string>

//compile with  -no-pie
int on_url(http_parser* parser, const char* at, size_t length) {
    http_parser_url url_data;
    http_parser_url_init(&url_data);
    http_parser_parse_url(at, length, 0, &url_data);
    std::string url(at);
    uint16_t size = url_data.field_data[UF_PATH].len;
    uint16_t from = url_data.field_data[UF_PATH].off;
    std::string path = url.substr(from, size);
    strncpy((char*)parser->data, path.c_str(), path.size());
    return 0;
}

using boost::asio::ip::tcp;

class Connection
{
public:
    Connection(boost::asio::io_service& service, std::string& dir) :m_socket(service), m_buffer(1024), m_dir(dir) {}
    tcp::socket& socket()
    {
        return m_socket;
    }
    void operator()()
    {
        std::size_t read = m_socket.read_some(boost::asio::buffer(&m_buffer[0], 1024));
        readHandler(read);
        boost::system::error_code erro;
        m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, erro);
        m_socket.close();
    }
private:
    void readHandler(std::size_t bytes_transferred)
    {
            std::cout << "input:" << std::endl;
            for (int i = 0; i < bytes_transferred; ++i)
                std::cout << m_buffer[i];
            std::cout << std::endl;

            std::string path = getPath(bytes_transferred);
            std::cout << "Path: " << path << std::endl;
            bool pathOk = path.size() > 1;
            std::ifstream file(m_dir + path);
            if (pathOk && file.is_open())
            {
                std::stringstream ss;
                std::stringstream fileStream;
                fileStream << file.rdbuf();
                std::string content(fileStream.str());
                ss << "HTTP/1.0 200 OK\r\n";
                ss << "Content-Type: text/html\r\n";
                ss << "Content-length: " << content.size() << "\r\n";
                ss << "Connection: close\r\n";
                ss << "\r\n";
                ss << content;
                std::string str(ss.str());
                file.close();
                std::cout << "Response: " << str << std::endl;
                m_socket.write_some(boost::asio::buffer(&str[0], str.size()));
            }
            else
            {
                std::string not_found("HTTP/1.0 404 NOT FOUND\r\nContent-length: 0\r\nContent-Type: text/html\r\n\r\n");
                std::cout << "Responce: " << not_found << std::endl;
                m_socket.write_some(boost::asio::buffer(&not_found[0], not_found.size()));
            }
            m_buffer.clear();
    }

private:
    std::string getPath(std::size_t bytes_transferred)
    {
        http_parser_settings in_settings;
        http_parser_settings_init(&in_settings);
        in_settings.on_url = on_url;

        char* in_parser_buffer = new char[255];
        memset(in_parser_buffer, 0, 255);
        http_parser in_parser;
        in_parser.data = in_parser_buffer;
        http_parser_init(&in_parser, HTTP_REQUEST);
        http_parser_execute(&in_parser, &in_settings, &m_buffer[0], bytes_transferred);
        return std::string(in_parser_buffer);
    }
private:
    tcp::socket m_socket;
    std::vector<char> m_buffer;
    std::string& m_dir;
};

class server
{
public:
    server(boost::asio::io_service& io_service, const std::string& host, short port, const std::string& dir)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port)), port_(port), host_(host), dir_(dir)
    {
        start_accept();
    }

private:
    void start_accept()
    {
        Connection* connection = new Connection(io_service_, dir_);
        acceptor_.async_accept(connection->socket(), boost::bind(&server::handle_accept, this, connection, boost::asio::placeholders::error));
    }

    void handle_accept(Connection* connection, const boost::system::error_code& error)
    {
        //std::cout << "New connection!" << std::endl;
        boost::thread thread(std::move(*connection));
        thread.detach();
        start_accept();
    }

    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
    int port_;
    std::string host_;
    std::string dir_;
};

int main(int argc, char* argv[])
{

    pid_t pid, sid;

    /* Fork off the parent process */
    pid = fork();
    if (pid < 0) {
            exit(EXIT_FAILURE);
    }
    /* If we got a good PID, then
       we can exit the parent process. */
    if (pid > 0) {
            exit(EXIT_SUCCESS);
    }

    /* Change the file mode mask */
    umask(0);

    /* Open any logs here */

    /* Create a new SID for the child process */
    sid = setsid();
    if (sid < 0) {
            /* Log any failure here */
            exit(EXIT_FAILURE);
    }

    /* Change the current working directory */
    if ((chdir("/")) < 0) {
            /* Log any failure here */
            exit(EXIT_FAILURE);
    }


    /* Close out the standard file descriptors */
    auto fd = open("/tmp/webserver.log", O_WRONLY|O_CREAT|O_APPEND, 0777);

    dup2(fd, STDOUT_FILENO);

    dup2(fd, STDERR_FILENO);

    close(STDIN_FILENO);

    if (fd > 2) close(fd);

    std::cout << "===================[ server start ]===================\n";
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    int port;
    std::string dir;
    std::string host;
    int c;
    while ((c = getopt (argc, argv, "h:p:d:")) != -1)
    switch (c)
        {
        case 'h':
          host = optarg;
          break;
        case 'p':
          port = atoi(optarg);
          break;
        case 'd':
          dir = optarg;
          break;
        default:
          abort ();
        }
    try
    {
        boost::asio::io_service io_service;

        server s(io_service, host, port, dir);

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cout << "Exception: " << e.what() << "\n";
    }

    return 0;
}
