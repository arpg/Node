/*
 *  Created on: 16 Aug 2011
 *      Author: Ben Gray (@benjamg)
 */

#include "exception.hpp"
#include "socket.hpp"
#include "reactor.hpp"
#include <algorithm>
#include <zmq.h>

namespace zmqpp
{

    reactor::reactor() :
    dispatching_(false)
    {

    }

    reactor::~reactor()
    {
    }

    void reactor::add(socket& socket, Callable callable, short const event /* = POLL_IN */)
    {
        zmq_pollitem_t item{static_cast<void *> (socket), 0, event, 0};
        add(item, callable);
    }

    void reactor::add(int const descriptor, Callable callable, short const event /* = POLL_IN */)
    {
        zmq_pollitem_t item{nullptr, descriptor, event, 0};
        add(item, callable);
    }

    void reactor::add(const zmq_pollitem_t& item, Callable callable)
    {
        poller_.add(item);

        items_.push_back(std::make_pair(item, callable));
    }

    bool reactor::has(socket_t const& socket)
    {
        return poller_.has(socket);
    }

    bool reactor::has(int const descriptor)
    {
        return poller_.has(descriptor);
    }

    void reactor::remove(socket_t const& socket)
    {
        if (dispatching_)
        {
            sockRemoveLater_.push_back(&socket);
            return;
        }
        items_.erase(std::remove_if(items_.begin(), items_.end(), [&socket](const PollItemCallablePair & pair) -> bool
        {
            const zmq_pollitem_t &item = pair.first;
            if (nullptr != item.socket && item.socket == static_cast<void *> (socket))
            {
                return true;
            }
            return false;
        }), items_.end());
        poller_.remove(socket);
    }

    void reactor::remove(int const descriptor)
    {
        if (dispatching_)
        {
            fdRemoveLater_.push_back(descriptor);
            return;
        }
        items_.erase(std::remove_if(items_.begin(), items_.end(), [descriptor](const PollItemCallablePair & pair) -> bool
        {
            const zmq_pollitem_t &item = pair.first;
            if (nullptr == item.socket && item.fd == descriptor)
            {
                return true;
            }
            return false;
        }), items_.end());
        poller_.remove(descriptor);
    }

    void reactor::check_for(socket const& socket, short const event)
    {
        poller_.check_for(socket, event);
    }

    void reactor::check_for(int const descriptor, short const event)
    {
        poller_.check_for(descriptor, event);
    }

    bool reactor::poll(long timeout /* = WAIT_FOREVER */)
    {
        if (poller_.poll(timeout))
        {
            dispatching_ = true;
            for (const PollItemCallablePair &pair : items_)
            {
                const zmq_pollitem_t &pollitem = pair.first;

                if (poller_.has_input(pollitem) || poller_.has_error(pollitem) || poller_.has_output(pollitem))
                    pair.second();
            }
            dispatching_ = false;
            flush_remove_later();
            return true;
        }
        return false;
    }

    short reactor::events(socket const& socket) const
    {
        return poller_.events(socket);
    }

    short reactor::events(int const descriptor) const
    {
        return poller_.events(descriptor);
    }

    poller& reactor::get_poller()
    {
        return poller_;
    }

    const poller& reactor::get_poller() const
    {
        return poller_;
    }

    void reactor::flush_remove_later()
    {
        for (int fd : fdRemoveLater_)
            remove(fd);
        for (const socket_t *sock : sockRemoveLater_)
            remove(*sock);
        fdRemoveLater_.clear();
        sockRemoveLater_.clear();
    }

}
