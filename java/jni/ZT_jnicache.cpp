//
// Created by Brenton Bostick on 1/18/23.
//

#include "ZT_jnicache.h"

#include "ZT_jniutils.h"

#include <cassert>

#define LOG_TAG "Cache"

#define EXCEPTIONANDNULLCHECK(var) \
    do { \
        if (env->ExceptionCheck()) { \
            assert(false && "Exception"); \
        } \
        if ((var) == NULL) { \
            assert(false && #var " is NULL"); \
        } \
    } while (false)

#define SETCLASS(classVar, classNameString) \
	do { \
        jclass classVar ## _local = env->FindClass(classNameString); \
        EXCEPTIONANDNULLCHECK(classVar ## _local); \
        classVar = reinterpret_cast<jclass>(env->NewGlobalRef(classVar ## _local)); \
        EXCEPTIONANDNULLCHECK(classVar); \
        env->DeleteLocalRef(classVar ## _local); \
    } while (false)

#define SETOBJECT(objectVar, code) \
	do { \
        jobject objectVar ## _local = code; \
        EXCEPTIONANDNULLCHECK(objectVar ## _local); \
        objectVar = env->NewGlobalRef(objectVar ## _local); \
        EXCEPTIONANDNULLCHECK(objectVar); \
        env->DeleteLocalRef(objectVar ## _local); \
    } while (false)


//
// Classes
//

jclass ArrayList_class;
jclass DataStoreGetListener_class;
jclass DataStorePutListener_class;
jclass EventListener_class;
jclass Event_class;
jclass Inet4Address_class;
jclass Inet6Address_class;
jclass InetAddress_class;
jclass InetSocketAddress_class;
jclass NodeStatus_class;
jclass Node_class;
jclass PacketSender_class;
jclass PathChecker_class;
jclass PeerPhysicalPath_class;
jclass PeerRole_class;
jclass Peer_class;
jclass ResultCode_class;
jclass Version_class;
jclass VirtualNetworkConfigListener_class;
jclass VirtualNetworkConfigOperation_class;
jclass VirtualNetworkConfig_class;
jclass VirtualNetworkDNS_class;
jclass VirtualNetworkFrameListener_class;
jclass VirtualNetworkRoute_class;
jclass VirtualNetworkStatus_class;
jclass VirtualNetworkType_class;

//
// Instance methods
//

jmethodID ArrayList_add_method;
jmethodID ArrayList_ctor;
jmethodID DataStoreGetListener_onDataStoreGet_method;
jmethodID DataStorePutListener_onDataStorePut_method;
jmethodID DataStorePutListener_onDelete_method;
jmethodID EventListener_onEvent_method;
jmethodID EventListener_onTrace_method;
jmethodID InetAddress_getAddress_method;
jmethodID InetSocketAddress_ctor;
jmethodID InetSocketAddress_getAddress_method;
jmethodID InetSocketAddress_getPort_method;
jmethodID NodeStatus_ctor;
jmethodID PacketSender_onSendPacketRequested_method;
jmethodID PathChecker_onPathCheck_method;
jmethodID PathChecker_onPathLookup_method;
jmethodID PeerPhysicalPath_ctor;
jmethodID Peer_ctor;
jmethodID Version_ctor;
jmethodID VirtualNetworkConfigListener_onNetworkConfigurationUpdated_method;
jmethodID VirtualNetworkConfig_ctor;
jmethodID VirtualNetworkDNS_ctor;
jmethodID VirtualNetworkFrameListener_onVirtualNetworkFrame_method;
jmethodID VirtualNetworkRoute_ctor;

//
// Static methods
//

jmethodID Event_fromInt_method;
jmethodID InetAddress_getByAddress_method;

//
// Instance fields
//

jfieldID NodeStatus_address_field;
jfieldID NodeStatus_online_field;
jfieldID NodeStatus_publicIdentity_field;
jfieldID NodeStatus_secretIdentity_field;
jfieldID Node_configListener_field;
jfieldID Node_eventListener_field;
jfieldID Node_frameListener_field;
jfieldID Node_getListener_field;
jfieldID Node_pathChecker_field;
jfieldID Node_putListener_field;
jfieldID Node_sender_field;
jfieldID PeerPhysicalPath_address_field;
jfieldID PeerPhysicalPath_lastReceive_field;
jfieldID PeerPhysicalPath_lastSend_field;
jfieldID PeerPhysicalPath_preferred_field;
jfieldID Peer_address_field;
jfieldID Peer_latency_field;
jfieldID Peer_paths_field;
jfieldID Peer_role_field;
jfieldID Peer_versionMajor_field;
jfieldID Peer_versionMinor_field;
jfieldID Peer_versionRev_field;
jfieldID Version_major_field;
jfieldID Version_minor_field;
jfieldID Version_revision_field;
jfieldID VirtualNetworkConfig_assignedAddresses_field;
jfieldID VirtualNetworkConfig_bridge_field;
jfieldID VirtualNetworkConfig_broadcastEnabled_field;
jfieldID VirtualNetworkConfig_dhcp_field;
jfieldID VirtualNetworkConfig_dns_field;
jfieldID VirtualNetworkConfig_enabled_field;
jfieldID VirtualNetworkConfig_mac_field;
jfieldID VirtualNetworkConfig_mtu_field;
jfieldID VirtualNetworkConfig_name_field;
jfieldID VirtualNetworkConfig_nwid_field;
jfieldID VirtualNetworkConfig_portError_field;
jfieldID VirtualNetworkConfig_routes_field;
jfieldID VirtualNetworkConfig_status_field;
jfieldID VirtualNetworkConfig_type_field;
jfieldID VirtualNetworkDNS_domain_field;
jfieldID VirtualNetworkDNS_servers_field;
jfieldID VirtualNetworkRoute_flags_field;
jfieldID VirtualNetworkRoute_metric_field;
jfieldID VirtualNetworkRoute_target_field;
jfieldID VirtualNetworkRoute_via_field;

//
// Static fields
//

jfieldID PeerRole_PEER_ROLE_LEAF_field;
jfieldID PeerRole_PEER_ROLE_MOON_field;
jfieldID PeerRole_PEER_ROLE_PLANET_field;
jfieldID ResultCode_RESULT_ERROR_BAD_PARAMETER_field;
jfieldID ResultCode_RESULT_ERROR_NETWORK_NOT_FOUND_field;
jfieldID ResultCode_RESULT_ERROR_UNSUPPORTED_OPERATION_field;
jfieldID ResultCode_RESULT_FATAL_ERROR_DATA_STORE_FAILED_field;
jfieldID ResultCode_RESULT_FATAL_ERROR_INTERNAL_field;
jfieldID ResultCode_RESULT_FATAL_ERROR_OUT_OF_MEMORY_field;
jfieldID ResultCode_RESULT_OK_field;
jfieldID VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE_field;
jfieldID VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY_field;
jfieldID VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN_field;
jfieldID VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_UP_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_ACCESS_DENIED_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_AUTHENTICATION_REQUIRED_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_CLIENT_TOO_OLD_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_NOT_FOUND_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_OK_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_PORT_ERROR_field;
jfieldID VirtualNetworkStatus_NETWORK_STATUS_REQUESTING_CONFIGURATION_field;
jfieldID VirtualNetworkType_NETWORK_TYPE_PRIVATE_field;
jfieldID VirtualNetworkType_NETWORK_TYPE_PUBLIC_field;

//
// Enums
//

jobject ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum;
jobject ResultCode_RESULT_OK_enum;

void setupJNICache(JavaVM *vm) {

    JNIEnv *env;
    GETENV(env, vm);

    //
    // Classes
    //

    SETCLASS(ArrayList_class, "java/util/ArrayList");
    SETCLASS(DataStoreGetListener_class, "com/zerotier/sdk/DataStoreGetListener");
    SETCLASS(DataStorePutListener_class, "com/zerotier/sdk/DataStorePutListener");
    SETCLASS(EventListener_class, "com/zerotier/sdk/EventListener");
    SETCLASS(Event_class, "com/zerotier/sdk/Event");
    SETCLASS(Inet4Address_class, "java/net/Inet4Address");
    SETCLASS(Inet6Address_class, "java/net/Inet6Address");
    SETCLASS(InetAddress_class, "java/net/InetAddress");
    SETCLASS(InetSocketAddress_class, "java/net/InetSocketAddress");
    SETCLASS(NodeStatus_class, "com/zerotier/sdk/NodeStatus");
    SETCLASS(Node_class, "com/zerotier/sdk/Node");
    SETCLASS(PacketSender_class, "com/zerotier/sdk/PacketSender");
    SETCLASS(PathChecker_class, "com/zerotier/sdk/PathChecker");
    SETCLASS(PeerPhysicalPath_class, "com/zerotier/sdk/PeerPhysicalPath");
    SETCLASS(PeerRole_class, "com/zerotier/sdk/PeerRole");
    SETCLASS(Peer_class, "com/zerotier/sdk/Peer");
    SETCLASS(ResultCode_class, "com/zerotier/sdk/ResultCode");
    SETCLASS(Version_class, "com/zerotier/sdk/Version");
    SETCLASS(VirtualNetworkConfigListener_class, "com/zerotier/sdk/VirtualNetworkConfigListener");
    SETCLASS(VirtualNetworkConfigOperation_class, "com/zerotier/sdk/VirtualNetworkConfigOperation");
    SETCLASS(VirtualNetworkConfig_class, "com/zerotier/sdk/VirtualNetworkConfig");
    SETCLASS(VirtualNetworkDNS_class, "com/zerotier/sdk/VirtualNetworkDNS");
    SETCLASS(VirtualNetworkFrameListener_class, "com/zerotier/sdk/VirtualNetworkFrameListener");
    SETCLASS(VirtualNetworkRoute_class, "com/zerotier/sdk/VirtualNetworkRoute");
    SETCLASS(VirtualNetworkStatus_class, "com/zerotier/sdk/VirtualNetworkStatus");
    SETCLASS(VirtualNetworkType_class, "com/zerotier/sdk/VirtualNetworkType");

    //
    // Instance methods
    //

    EXCEPTIONANDNULLCHECK(ArrayList_add_method = env->GetMethodID(ArrayList_class, "add", "(Ljava/lang/Object;)Z"));
    EXCEPTIONANDNULLCHECK(ArrayList_ctor = env->GetMethodID(ArrayList_class, "<init>", "(I)V"));
    EXCEPTIONANDNULLCHECK(DataStoreGetListener_onDataStoreGet_method = env->GetMethodID(DataStoreGetListener_class, "onDataStoreGet", "(Ljava/lang/String;[B)J"));
    EXCEPTIONANDNULLCHECK(DataStorePutListener_onDataStorePut_method = env->GetMethodID(DataStorePutListener_class, "onDataStorePut", "(Ljava/lang/String;[BZ)I"));
    EXCEPTIONANDNULLCHECK(DataStorePutListener_onDelete_method = env->GetMethodID(DataStorePutListener_class, "onDelete", "(Ljava/lang/String;)I"));
    EXCEPTIONANDNULLCHECK(EventListener_onEvent_method = env->GetMethodID(EventListener_class, "onEvent", "(Lcom/zerotier/sdk/Event;)V"));
    EXCEPTIONANDNULLCHECK(EventListener_onTrace_method = env->GetMethodID(EventListener_class, "onTrace", "(Ljava/lang/String;)V"));
    EXCEPTIONANDNULLCHECK(InetAddress_getAddress_method = env->GetMethodID(InetAddress_class, "getAddress", "()[B"));
    EXCEPTIONANDNULLCHECK(InetSocketAddress_ctor = env->GetMethodID(InetSocketAddress_class, "<init>", "(Ljava/net/InetAddress;I)V"));
    EXCEPTIONANDNULLCHECK(InetSocketAddress_getAddress_method = env->GetMethodID(InetSocketAddress_class, "getAddress", "()Ljava/net/InetAddress;"));
    EXCEPTIONANDNULLCHECK(InetSocketAddress_getPort_method = env->GetMethodID(InetSocketAddress_class, "getPort", "()I"));
    EXCEPTIONANDNULLCHECK(NodeStatus_ctor = env->GetMethodID(NodeStatus_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(PacketSender_onSendPacketRequested_method = env->GetMethodID(PacketSender_class, "onSendPacketRequested", "(JLjava/net/InetSocketAddress;[BI)I"));
    EXCEPTIONANDNULLCHECK(PathChecker_onPathCheck_method = env->GetMethodID(PathChecker_class, "onPathCheck", "(JJLjava/net/InetSocketAddress;)Z"));
    EXCEPTIONANDNULLCHECK(PathChecker_onPathLookup_method = env->GetMethodID(PathChecker_class, "onPathLookup", "(JI)Ljava/net/InetSocketAddress;"));
    EXCEPTIONANDNULLCHECK(PeerPhysicalPath_ctor = env->GetMethodID(PeerPhysicalPath_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(Peer_ctor = env->GetMethodID(Peer_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(Version_ctor = env->GetMethodID(Version_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfigListener_onNetworkConfigurationUpdated_method = env->GetMethodID(VirtualNetworkConfigListener_class, "onNetworkConfigurationUpdated", "(JLcom/zerotier/sdk/VirtualNetworkConfigOperation;Lcom/zerotier/sdk/VirtualNetworkConfig;)I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_ctor = env->GetMethodID(VirtualNetworkConfig_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkDNS_ctor = env->GetMethodID(VirtualNetworkDNS_class, "<init>", "()V"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkFrameListener_onVirtualNetworkFrame_method = env->GetMethodID(VirtualNetworkFrameListener_class, "onVirtualNetworkFrame", "(JJJJJ[B)V"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkRoute_ctor = env->GetMethodID(VirtualNetworkRoute_class, "<init>", "()V"));

    //
    // Static methods
    //

    EXCEPTIONANDNULLCHECK(Event_fromInt_method = env->GetStaticMethodID(Event_class, "fromInt", "(I)Lcom/zerotier/sdk/Event;"));
    EXCEPTIONANDNULLCHECK(InetAddress_getByAddress_method = env->GetStaticMethodID(InetAddress_class, "getByAddress", "([B)Ljava/net/InetAddress;"));

    //
    // Instance fields
    //

    EXCEPTIONANDNULLCHECK(NodeStatus_address_field = env->GetFieldID(NodeStatus_class, "address", "J"));
    EXCEPTIONANDNULLCHECK(NodeStatus_online_field = env->GetFieldID(NodeStatus_class, "online", "Z"));
    EXCEPTIONANDNULLCHECK(NodeStatus_publicIdentity_field = env->GetFieldID(NodeStatus_class, "publicIdentity", "Ljava/lang/String;"));
    EXCEPTIONANDNULLCHECK(NodeStatus_secretIdentity_field = env->GetFieldID(NodeStatus_class, "secretIdentity", "Ljava/lang/String;"));
    EXCEPTIONANDNULLCHECK(Node_configListener_field = env->GetFieldID(Node_class, "configListener", "Lcom/zerotier/sdk/VirtualNetworkConfigListener;"));
    EXCEPTIONANDNULLCHECK(Node_eventListener_field = env->GetFieldID(Node_class, "eventListener", "Lcom/zerotier/sdk/EventListener;"));
    EXCEPTIONANDNULLCHECK(Node_frameListener_field = env->GetFieldID(Node_class, "frameListener", "Lcom/zerotier/sdk/VirtualNetworkFrameListener;"));
    EXCEPTIONANDNULLCHECK(Node_getListener_field = env->GetFieldID(Node_class, "getListener", "Lcom/zerotier/sdk/DataStoreGetListener;"));
    EXCEPTIONANDNULLCHECK(Node_pathChecker_field = env->GetFieldID(Node_class, "pathChecker", "Lcom/zerotier/sdk/PathChecker;"));
    EXCEPTIONANDNULLCHECK(Node_putListener_field = env->GetFieldID(Node_class, "putListener", "Lcom/zerotier/sdk/DataStorePutListener;"));
    EXCEPTIONANDNULLCHECK(Node_sender_field = env->GetFieldID(Node_class, "sender", "Lcom/zerotier/sdk/PacketSender;"));
    EXCEPTIONANDNULLCHECK(PeerPhysicalPath_address_field = env->GetFieldID(PeerPhysicalPath_class, "address", "Ljava/net/InetSocketAddress;"));
    EXCEPTIONANDNULLCHECK(PeerPhysicalPath_lastReceive_field = env->GetFieldID(PeerPhysicalPath_class, "lastReceive", "J"));
    EXCEPTIONANDNULLCHECK(PeerPhysicalPath_lastSend_field = env->GetFieldID(PeerPhysicalPath_class, "lastSend", "J"));
    EXCEPTIONANDNULLCHECK(PeerPhysicalPath_preferred_field = env->GetFieldID(PeerPhysicalPath_class, "preferred", "Z"));
    EXCEPTIONANDNULLCHECK(Peer_address_field = env->GetFieldID(Peer_class, "address", "J"));
    EXCEPTIONANDNULLCHECK(Peer_latency_field = env->GetFieldID(Peer_class, "latency", "I"));
    EXCEPTIONANDNULLCHECK(Peer_paths_field = env->GetFieldID(Peer_class, "paths", "[Lcom/zerotier/sdk/PeerPhysicalPath;"));
    EXCEPTIONANDNULLCHECK(Peer_role_field = env->GetFieldID(Peer_class, "role", "Lcom/zerotier/sdk/PeerRole;"));
    EXCEPTIONANDNULLCHECK(Peer_versionMajor_field = env->GetFieldID(Peer_class, "versionMajor", "I"));
    EXCEPTIONANDNULLCHECK(Peer_versionMinor_field = env->GetFieldID(Peer_class, "versionMinor", "I"));
    EXCEPTIONANDNULLCHECK(Peer_versionRev_field = env->GetFieldID(Peer_class, "versionRev", "I"));
    EXCEPTIONANDNULLCHECK(Version_major_field = env->GetFieldID(Version_class, "major", "I"));
    EXCEPTIONANDNULLCHECK(Version_minor_field = env->GetFieldID(Version_class, "minor", "I"));
    EXCEPTIONANDNULLCHECK(Version_revision_field = env->GetFieldID(Version_class, "revision", "I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_assignedAddresses_field = env->GetFieldID(VirtualNetworkConfig_class, "assignedAddresses", "[Ljava/net/InetSocketAddress;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_bridge_field = env->GetFieldID(VirtualNetworkConfig_class, "bridge", "Z"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_broadcastEnabled_field = env->GetFieldID(VirtualNetworkConfig_class, "broadcastEnabled", "Z"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_dhcp_field = env->GetFieldID(VirtualNetworkConfig_class, "dhcp", "Z"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_dns_field = env->GetFieldID(VirtualNetworkConfig_class, "dns", "Lcom/zerotier/sdk/VirtualNetworkDNS;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_enabled_field = env->GetFieldID(VirtualNetworkConfig_class, "enabled", "Z"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_mac_field = env->GetFieldID(VirtualNetworkConfig_class, "mac", "J"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_mtu_field = env->GetFieldID(VirtualNetworkConfig_class, "mtu", "I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_name_field = env->GetFieldID(VirtualNetworkConfig_class, "name", "Ljava/lang/String;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_nwid_field = env->GetFieldID(VirtualNetworkConfig_class, "nwid", "J"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_portError_field = env->GetFieldID(VirtualNetworkConfig_class, "portError", "I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_routes_field = env->GetFieldID(VirtualNetworkConfig_class, "routes", "[Lcom/zerotier/sdk/VirtualNetworkRoute;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_status_field = env->GetFieldID(VirtualNetworkConfig_class, "status", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfig_type_field = env->GetFieldID(VirtualNetworkConfig_class, "type", "Lcom/zerotier/sdk/VirtualNetworkType;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkDNS_domain_field = env->GetFieldID(VirtualNetworkDNS_class, "domain", "Ljava/lang/String;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkDNS_servers_field = env->GetFieldID(VirtualNetworkDNS_class, "servers", "Ljava/util/ArrayList;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkRoute_flags_field = env->GetFieldID(VirtualNetworkRoute_class, "flags", "I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkRoute_metric_field = env->GetFieldID(VirtualNetworkRoute_class, "metric", "I"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkRoute_target_field = env->GetFieldID(VirtualNetworkRoute_class, "target", "Ljava/net/InetSocketAddress;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkRoute_via_field = env->GetFieldID(VirtualNetworkRoute_class, "via", "Ljava/net/InetSocketAddress;"));

    //
    // Static fields
    //

    EXCEPTIONANDNULLCHECK(PeerRole_PEER_ROLE_LEAF_field = env->GetStaticFieldID(PeerRole_class, "PEER_ROLE_LEAF", "Lcom/zerotier/sdk/PeerRole;"));
    EXCEPTIONANDNULLCHECK(PeerRole_PEER_ROLE_MOON_field = env->GetStaticFieldID(PeerRole_class, "PEER_ROLE_MOON", "Lcom/zerotier/sdk/PeerRole;"));
    EXCEPTIONANDNULLCHECK(PeerRole_PEER_ROLE_PLANET_field = env->GetStaticFieldID(PeerRole_class, "PEER_ROLE_PLANET", "Lcom/zerotier/sdk/PeerRole;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_ERROR_BAD_PARAMETER_field = env->GetStaticFieldID(ResultCode_class, "RESULT_ERROR_BAD_PARAMETER", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_ERROR_NETWORK_NOT_FOUND_field = env->GetStaticFieldID(ResultCode_class, "RESULT_ERROR_NETWORK_NOT_FOUND", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_ERROR_UNSUPPORTED_OPERATION_field = env->GetStaticFieldID(ResultCode_class, "RESULT_ERROR_UNSUPPORTED_OPERATION", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_FATAL_ERROR_DATA_STORE_FAILED_field = env->GetStaticFieldID(ResultCode_class, "RESULT_FATAL_ERROR_DATA_STORE_FAILED", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_FATAL_ERROR_INTERNAL_field = env->GetStaticFieldID(ResultCode_class, "RESULT_FATAL_ERROR_INTERNAL", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_FATAL_ERROR_OUT_OF_MEMORY_field = env->GetStaticFieldID(ResultCode_class, "RESULT_FATAL_ERROR_OUT_OF_MEMORY", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(ResultCode_RESULT_OK_field = env->GetStaticFieldID(ResultCode_class, "RESULT_OK", "Lcom/zerotier/sdk/ResultCode;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE_field = env->GetStaticFieldID(VirtualNetworkConfigOperation_class, "VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE", "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY_field = env->GetStaticFieldID(VirtualNetworkConfigOperation_class, "VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY", "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN_field = env->GetStaticFieldID(VirtualNetworkConfigOperation_class, "VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN", "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkConfigOperation_VIRTUAL_NETWORK_CONFIG_OPERATION_UP_field = env->GetStaticFieldID(VirtualNetworkConfigOperation_class, "VIRTUAL_NETWORK_CONFIG_OPERATION_UP", "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_ACCESS_DENIED_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_ACCESS_DENIED", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_AUTHENTICATION_REQUIRED_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_AUTHENTICATION_REQUIRED", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_CLIENT_TOO_OLD_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_CLIENT_TOO_OLD", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_NOT_FOUND_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_NOT_FOUND", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_OK_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_OK", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_PORT_ERROR_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_PORT_ERROR", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkStatus_NETWORK_STATUS_REQUESTING_CONFIGURATION_field = env->GetStaticFieldID(VirtualNetworkStatus_class, "NETWORK_STATUS_REQUESTING_CONFIGURATION", "Lcom/zerotier/sdk/VirtualNetworkStatus;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkType_NETWORK_TYPE_PRIVATE_field = env->GetStaticFieldID(VirtualNetworkType_class, "NETWORK_TYPE_PRIVATE", "Lcom/zerotier/sdk/VirtualNetworkType;"));
    EXCEPTIONANDNULLCHECK(VirtualNetworkType_NETWORK_TYPE_PUBLIC_field = env->GetStaticFieldID(VirtualNetworkType_class, "NETWORK_TYPE_PUBLIC", "Lcom/zerotier/sdk/VirtualNetworkType;"));

    //
    // Enums
    //

    SETOBJECT(ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum, createResultObject(env, ZT_RESULT_FATAL_ERROR_INTERNAL));
    SETOBJECT(ResultCode_RESULT_OK_enum, createResultObject(env, ZT_RESULT_OK));
}

void teardownJNICache(JavaVM *vm) {

    JNIEnv *env;
    GETENV(env, vm);

    env->DeleteGlobalRef(ArrayList_class);
    env->DeleteGlobalRef(DataStoreGetListener_class);
    env->DeleteGlobalRef(DataStorePutListener_class);
    env->DeleteGlobalRef(EventListener_class);
    env->DeleteGlobalRef(Event_class);
    env->DeleteGlobalRef(InetAddress_class);
    env->DeleteGlobalRef(InetSocketAddress_class);
    env->DeleteGlobalRef(NodeStatus_class);
    env->DeleteGlobalRef(Node_class);
    env->DeleteGlobalRef(PacketSender_class);
    env->DeleteGlobalRef(PathChecker_class);
    env->DeleteGlobalRef(PeerPhysicalPath_class);
    env->DeleteGlobalRef(PeerRole_class);
    env->DeleteGlobalRef(Peer_class);
    env->DeleteGlobalRef(ResultCode_class);
    env->DeleteGlobalRef(Version_class);
    env->DeleteGlobalRef(VirtualNetworkConfigListener_class);
    env->DeleteGlobalRef(VirtualNetworkConfigOperation_class);
    env->DeleteGlobalRef(VirtualNetworkConfig_class);
    env->DeleteGlobalRef(VirtualNetworkDNS_class);
    env->DeleteGlobalRef(VirtualNetworkFrameListener_class);
    env->DeleteGlobalRef(VirtualNetworkRoute_class);
    env->DeleteGlobalRef(VirtualNetworkStatus_class);
    env->DeleteGlobalRef(VirtualNetworkType_class);

    env->DeleteGlobalRef(ResultCode_RESULT_FATAL_ERROR_INTERNAL_enum);
    env->DeleteGlobalRef(ResultCode_RESULT_OK_enum);
}
