#include <iostream>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <vector>

using boost::asio::ip::tcp;

class Connection
{
public:
    Connection(boost::asio::io_service& service) :_service(service), m_socket(service), m_buffer(1024) {}
    tcp::socket& socket()
    {
        return m_socket;
    }
    void operator()()
    {
        m_socket.async_read_some(boost::asio::buffer(&m_buffer[0], 1024), boost::bind(&Connection::readHandler, this, _1, _2));
        _service.run();
    }
private:
    void readHandler(const boost::system::error_code& error, std::size_t bytes_transferred)
    {
        if (!error)
        {
            m_socket.async_write_some(boost::asio::buffer(&m_buffer[0], bytes_transferred), boost::bind(&Connection::writeHandler, this, _1, _2));
            for (int i = 0; i < bytes_transferred; ++i)
                std::cout << m_buffer[i];
            std::cout << std::endl;
        }
    }
    void writeHandler(const boost::system::error_code& error, std::size_t bytes_transferred)
    {
        if (!error)
        {
            m_socket.async_read_some(boost::asio::buffer(&m_buffer[0], 1024), boost::bind(&Connection::readHandler, this, _1, _2));
        }
    }
private:
    boost::asio::io_service& _service;
    tcp::socket m_socket;
    std::vector<char> m_buffer;
};

class server
{
public:
    server(boost::asio::io_service& io_service, short port)
        : io_service_(io_service), acceptor_(io_service, tcp::endpoint(tcp::v4(), port))
    {
        start_accept();
    }

private:
    void start_accept()
    {
        Connection* connection = new Connection(io_service_);
        acceptor_.async_accept(connection->socket(), boost::bind(&server::handle_accept, this, connection, boost::asio::placeholders::error));
    }

    void handle_accept(Connection* connection, const boost::system::error_code& error)
    {
        std::cout << "New connection!" << std::endl;
        boost::thread thread(std::move(*connection));
        thread.detach();
        start_accept();
    }

    boost::asio::io_service& io_service_;
    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
    try
    {
        boost::asio::io_service io_service;

        server s(io_service, 1234);

        io_service.run();
    }
    catch (std::exception& e)
    {
        std::cerr << "Exception: " << e.what() << "\n";
    }

    return 0;
}
