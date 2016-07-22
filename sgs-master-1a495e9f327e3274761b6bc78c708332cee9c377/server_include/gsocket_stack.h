/***************************************************************************
 ***************************************************************************
  (c) 1999-2006 Ganymede Technologies                 All Rights Reserved
      Krakow, Poland                                  www.ganymede.eu
 ***************************************************************************
 ***************************************************************************/

#define SOCKET_NONE	0

class GSocketStack: private boost::noncopyable
{
public: 
	GSocketStack()
	{

	}
	void Init(int count)
	{
		int a;
		for (a=0;a<count;a++)
		{
			socket_stack.push(NET_DUP_START+a);
		}
	}
	SOCKET Pop()
	{
		boost::mutex::scoped_lock lock(mtx);
		if(socket_stack.size()==0) return SOCKET_NONE;
		SOCKET socket=socket_stack.top();
		socket_stack.pop();
		return socket;
	}
	void Push(SOCKET socket)
	{
		if(socket==0) return;
		boost::mutex::scoped_lock lock(mtx);
		socket_stack.push(socket);
	}
private:
	stack<SOCKET>				socket_stack;
	boost::mutex				mtx;
};


