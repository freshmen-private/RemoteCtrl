#include "pch.h"
#include "ENetwork.h"

EServer::EServer(const EServerParamter& param) : m_stop(false), m_args(NULL)
{
	m_params = param;
	m_thread.UpdateWorker(ThreadWorker(this, (FUNCTYPE)&EServer::threadFunc));
}

EServer::~EServer()
{
	stop();
}

int EServer::Invoke(void* arg)
{
    m_sock.reset(new ESocket(m_params.m_type));
    if (*m_sock == INVALID_SOCKET)
    {
        printf("%s,(%d):%s %d\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
        return -1;
    }
    if (m_params.m_type == ETYPE::ETypeTCP)
    {
        if (m_sock->listen() == -1) return -2;
    }
    ESockaddrIn client;
    if (-1 == m_sock->bind(m_params.m_ip, m_params.m_port))
    {
        printf("%s,(%d):%s %d\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
        return -3;
    }
    if (m_thread.Start() == false) return -4;
    m_args = arg;
	return 0;
}

int EServer::Send(ESOCKET& client, const EBuffer& buffer)
{
    int ret = m_sock->send(buffer);//TODO 待优化，发送虽然成功，但是不完整
    if (m_params.m_send) m_params.m_send(m_args, client, ret);
    return ret;
}

int EServer::Sendto(ESockaddrIn& addr, const EBuffer& buffer)
{
    int ret = m_sock->sendto(buffer, addr);
    if (m_params.m_sendt) m_params.m_sendt(m_args, addr, ret);
    return ret;
}

int EServer::stop()
{
    if (m_stop == false)
    {
        m_sock->close();
        m_stop = true;
        m_thread.Stop();
    }
    return 0;
}

int EServer::threadFunc()
{
    if (m_params.m_type == ETYPE::ETypeTCP)
    {
        return threadTCPFunc();
    }
    else
    {
        return threadUDPFunc();
    }
    
}

int EServer::threadUDPFunc()
{
    EBuffer buf(1024 * 256);
    ESockaddrIn client;
    int ret = 0;
    while (!m_stop)
    {
        ret = m_sock->recvfrom(buf, client);
        if (ret > 0)
        {
            client.update();
            if (m_params.m_recvf != NULL)
            {
                m_params.m_recvf(m_args, buf, client);
            }
        }
        else
        {
            printf("%s,(%d):%s %d\n", __FILE__, __LINE__, __FUNCTION__, WSAGetLastError());
            break;
        }
    }
    if (m_stop == false) m_stop = true;
    m_sock->close();
    printf("%s,(%d):%s\n", __FILE__, __LINE__, __FUNCTION__);
    return 0;
}

int EServer::threadTCPFunc()
{
    return 0;
}

EServerParamter::EServerParamter(
    const std::string& ip, 
    short port, ETYPE type, 
    AcceptFunc acceptf, 
    RecvFunc recvf, 
    SendFunc sendf, 
    RecvFromFunc recvfromf, 
    SendToFunc sendtof)
{
    m_ip = ip;
    m_port = port;
    m_accept = acceptf;
    m_recv = recvf;
    m_send = sendf;
    m_recvf = recvfromf;
    m_sendt = sendtof;

}

EServerParamter& EServerParamter::operator<<(AcceptFunc func)
{
    m_accept = func;
    return *this;
}

EServerParamter& EServerParamter::operator<<(RecvFunc func)
{
    m_recv = func;
    return *this;
}

EServerParamter& EServerParamter::operator<<(SendFunc func)
{
    m_send = func;
    return *this;
}

EServerParamter& EServerParamter::operator<<(RecvFromFunc func)
{
    m_recvf = func;
    return *this;
}

EServerParamter& EServerParamter::operator<<(SendToFunc func)
{
    m_sendt = func;
    return *this;
}

EServerParamter& EServerParamter::operator<<(const std::string& ip)
{
    m_ip = ip;
    return *this;
}

EServerParamter& EServerParamter::operator<<(short port)
{
    m_port = port;
    return *this;
}

EServerParamter& EServerParamter::operator<<(ETYPE type)
{
    m_type = type;
    return *this;
}

EServerParamter& EServerParamter::operator>>(AcceptFunc& func)
{
    func = m_accept;
    return *this;
}

EServerParamter& EServerParamter::operator>>(RecvFunc& func)
{
    func = m_recv;
    return *this;
}

EServerParamter& EServerParamter::operator>>(SendFunc& func)
{
    func = m_send;
    return *this;
}

EServerParamter& EServerParamter::operator>>(RecvFromFunc& func)
{
    func = m_recvf;
    return *this;
}

EServerParamter& EServerParamter::operator>>(SendToFunc& func)
{
    func = m_sendt;
    return *this;
}

EServerParamter& EServerParamter::operator>>(std::string& ip)
{
    ip = m_ip;
    return *this;
}

EServerParamter& EServerParamter::operator>>(short& port)
{
    port = m_port;
    return *this;
}

EServerParamter& EServerParamter::operator>>(ETYPE& type)
{
    type = m_type;
    return *this;
}

EServerParamter::EServerParamter(const EServerParamter& param)
{
    m_ip = param.m_ip;
    m_port = param.m_port;
    m_type = param.m_type;
    m_accept = param.m_accept;
    m_recv = param.m_recv;
    m_send = param.m_send;
    m_recvf = param.m_recvf;
    m_sendt = param.m_sendt;
}

EServerParamter& EServerParamter::operator=(const EServerParamter& param)
{
    if(this != &param)
    {
        m_ip = param.m_ip;
        m_port = param.m_port;
        m_type = param.m_type;
        m_accept = param.m_accept;
        m_recv = param.m_recv;
        m_send = param.m_send;
        m_recvf = param.m_recvf;
        m_sendt = param.m_sendt;
    }
    return *this;
}
