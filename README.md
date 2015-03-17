Node
====

Node is a simple serverless pub/sub and rpc communication library.

The API is simple, e.g.,:

    main()
    {
        node::node n("my_node");
        ...
        n.subscribe( "other_node/other_topic" );
        ...
        n.advertise( "my_topic" );
        ...
        n.provide_rpc( "my_rpc_method", <function>, <user_data> );
        ...
        n.call_rpc( "other_node/rpc_method", <input>, <output> );
        ...
        n.publish( "my_topic", <message> );
        ...
        n.receive( "other_node/other_topic", <message> );
        ...
        n.register_callback( "other_node/other_topic", <callback_function> );
    }


Node uses:

1) ZeroMQ for it's messaging layer.

2) Protocol buffers for it's XDR and data marshalling.

3) Zeroconf for peer discovery.

In the near future we would like to replace our distriubted state
synchronization system with something like Raft or Replicant.


