#include "ZT1_jniutils.h"
#include "ZT1_jnicache.h"
#include <string>
#include <assert.h>

extern JniCache cache;

#ifdef __cplusplus
extern "C" {
#endif

jobject createResultObject(JNIEnv *env, ZT1_ResultCode code)
{
    jclass resultClass = NULL;
    
    jobject resultObject = NULL;

    resultClass = cache.findClass("com/zerotier/sdk/ResultCode");
    if(resultClass == NULL)
    {
        LOGE("Couldnt find ResultCode class");
        return NULL; // exception thrown
    }

    std::string fieldName;
    switch(code)
    {
    case ZT1_RESULT_OK:
        LOGV("ZT1_RESULT_OK");
        fieldName = "RESULT_OK";
        break;
    case ZT1_RESULT_FATAL_ERROR_OUT_OF_MEMORY:
        LOGV("ZT1_RESULT_FATAL_ERROR_OUT_OF_MEMORY");
        fieldName = "RESULT_FATAL_ERROR_OUT_OF_MEMORY";
        break;
    case ZT1_RESULT_FATAL_ERROR_DATA_STORE_FAILED:
        LOGV("RESULT_FATAL_ERROR_DATA_STORE_FAILED");
        fieldName = "RESULT_FATAL_ERROR_DATA_STORE_FAILED";
        break;
    case ZT1_RESULT_ERROR_NETWORK_NOT_FOUND:
        LOGV("RESULT_FATAL_ERROR_DATA_STORE_FAILED");
        fieldName = "RESULT_ERROR_NETWORK_NOT_FOUND";
        break;
    case ZT1_RESULT_FATAL_ERROR_INTERNAL:
    default:
        LOGV("RESULT_FATAL_ERROR_DATA_STORE_FAILED");
        fieldName = "RESULT_FATAL_ERROR_INTERNAL";
        break;
    }

    jfieldID enumField = cache.findStaticField(resultClass, fieldName.c_str(), "Lcom/zerotier/sdk/ResultCode;");
    if(env->ExceptionCheck() || enumField == NULL) 
    {
        LOGE("Error on FindStaticField");
        return NULL;
    }

    resultObject = env->GetStaticObjectField(resultClass, enumField);
    if(env->ExceptionCheck() || resultObject == NULL) 
    {
        LOGE("Error on GetStaticObjectField");
    }
    return resultObject;
}


jobject createVirtualNetworkStatus(JNIEnv *env, ZT1_VirtualNetworkStatus status)
{
    jobject statusObject = NULL;

    jclass statusClass = cache.findClass("com/zerotier/sdk/VirtualNetworkStatus");
    if(statusClass == NULL)
    {
        return NULL; // exception thrown
    }

    std::string fieldName;
    switch(status)
    {
    case ZT1_NETWORK_STATUS_REQUESTING_CONFIGURATION:
        fieldName = "NETWORK_STATUS_REQUESTING_CONFIGURATION";
        break;
    case ZT1_NETWORK_STATUS_OK:
        fieldName = "NETWORK_STATUS_OK";
        break;
    case ZT1_NETWORK_STATUS_ACCESS_DENIED:
        fieldName = "NETWORK_STATUS_ACCESS_DENIED";
        break;
    case ZT1_NETWORK_STATUS_NOT_FOUND:
        fieldName = "NETWORK_STATUS_NOT_FOUND";
        break;
    case ZT1_NETWORK_STATUS_PORT_ERROR:
        fieldName = "NETWORK_STATUS_PORT_ERROR";
        break;
    case ZT1_NETWORK_STATUS_CLIENT_TOO_OLD:
        fieldName = "NETWORK_STATUS_CLIENT_TOO_OLD";
        break;
    }

    jfieldID enumField = cache.findStaticField(statusClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkStatus;");

    statusObject = env->GetStaticObjectField(statusClass, enumField);

    return statusObject;
}

jobject createEvent(JNIEnv *env, ZT1_Event event)
{
    jclass eventClass = NULL;
    jobject eventObject = NULL;

    eventClass = cache.findClass("com/zerotier/sdk/Event");
    if(eventClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(event)
    {
    case ZT1_EVENT_UP:
        fieldName = "EVENT_UP";
        break;
    case ZT1_EVENT_OFFLINE:
        fieldName = "EVENT_OFFLINE";
        break;
    case ZT1_EVENT_ONLINE:
        fieldName = "EVENT_ONLINE";
        break;
    case ZT1_EVENT_DOWN:
        fieldName = "EVENT_DOWN";
        break;
    case ZT1_EVENT_FATAL_ERROR_IDENTITY_COLLISION:
        fieldName = "EVENT_FATAL_ERROR_IDENTITY_COLLISION";
        break;
    case ZT1_EVENT_SAW_MORE_RECENT_VERSION:
        fieldName = "EVENT_SAW_MORE_RECENT_VERSION";
        break;
    case ZT1_EVENT_AUTHENTICATION_FAILURE:
        fieldName = "EVENT_AUTHENTICATION_FAILURE";
        break;
    case ZT1_EVENT_INVALID_PACKET:
        fieldName = "EVENT_INVALID_PACKET";
        break;
    case ZT1_EVENT_TRACE:
        fieldName = "EVENT_TRACE";
        break;
    }

    jfieldID enumField = cache.findStaticField(eventClass, fieldName.c_str(), "Lcom/zerotier/sdk/Event;");

    eventObject = env->GetStaticObjectField(eventClass, enumField);

    return eventObject;
}

jobject createPeerRole(JNIEnv *env, ZT1_PeerRole role)
{
    jclass peerRoleClass = NULL;
    jobject peerRoleObject = NULL;

    peerRoleClass = cache.findClass("com/zerotier/sdk/PeerRole");
    if(peerRoleClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(role)
    {
    case ZT1_PEER_ROLE_LEAF:
        fieldName = "PEER_ROLE_LEAF";
        break;
    case ZT1_PEER_ROLE_HUB:
        fieldName = "PEER_ROLE_HUB";
        break;
    case ZT1_PEER_ROLE_ROOTSERVER:
        fieldName = "PEER_ROLE_ROOTSERVER";
        break;
    }

    jfieldID enumField = cache.findStaticField(peerRoleClass, fieldName.c_str(), "Lcom/zerotier/sdk/PeerRole;");

    peerRoleObject = env->GetStaticObjectField(peerRoleClass, enumField);

    return peerRoleObject;
}

jobject createVirtualNetworkType(JNIEnv *env, ZT1_VirtualNetworkType type)
{
    jclass vntypeClass = NULL;
    jobject vntypeObject = NULL;

    vntypeClass = cache.findClass("com/zerotier/sdk/VirtualNetworkType");
    if(env->ExceptionCheck() || vntypeClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(type)
    {
    case ZT1_NETWORK_TYPE_PRIVATE:
        fieldName = "NETWORK_TYPE_PRIVATE";
        break;
    case ZT1_NETWORK_TYPE_PUBLIC:
        fieldName = "NETWORK_TYPE_PUBLIC";
        break;
    }

    jfieldID enumField = cache.findStaticField(vntypeClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkType;");
    vntypeObject = env->GetStaticObjectField(vntypeClass, enumField);
    return vntypeObject;
}

jobject createVirtualNetworkConfigOperation(JNIEnv *env, ZT1_VirtualNetworkConfigOperation op)
{
    jclass vnetConfigOpClass = NULL;
    jobject vnetConfigOpObject = NULL;

    vnetConfigOpClass = cache.findClass("com/zerotier/sdk/VirtualNetworkConfigOperation");
    if(env->ExceptionCheck() || vnetConfigOpClass == NULL)
    {
        return NULL;
    }

    std::string fieldName;
    switch(op)
    {
    case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_UP:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_UP";
        break;
    case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_CONFIG_UPDATE";
        break;
    case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_DOWN";
        break;
    case ZT1_VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY:
        fieldName = "VIRTUAL_NETWORK_CONFIG_OPERATION_DESTROY";
        break;
    }

    jfieldID enumField = cache.findStaticField(vnetConfigOpClass, fieldName.c_str(), "Lcom/zerotier/sdk/VirtualNetworkConfigOperation;");
    vnetConfigOpObject = env->GetStaticObjectField(vnetConfigOpClass, enumField);
    return vnetConfigOpObject;
}

jobject newInetAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    LOGV("newInetAddress");
    jclass inetAddressClass = NULL;
    jmethodID inetAddress_getByAddress = NULL;

    inetAddressClass = cache.findClass("java/net/InetAddress");
    if(env->ExceptionCheck() || inetAddressClass == NULL)
    {
        LOGE("Error finding InetAddress class");
        return NULL;
    }

    inetAddress_getByAddress = cache.findStaticMethod(
        inetAddressClass, "getByAddress", "([B)Ljava/net/InetAddress;");
    if(env->ExceptionCheck() || inetAddress_getByAddress == NULL)
    {
        LOGE("Erorr finding getByAddress() static method");
        return NULL;
    }

    jobject inetAddressObj = NULL;
    switch(addr.ss_family)
    {
        case AF_INET6:
        {
            sockaddr_in6 *ipv6 = (sockaddr_in6*)&addr;
            jbyteArray buff = env->NewByteArray(16);
            if(buff == NULL)
            {
                LOGE("Error creating IPV6 byte array");
                return NULL;
            }

            env->SetByteArrayRegion(buff, 0, 16, (jbyte*)ipv6->sin6_addr.s6_addr);
            inetAddressObj = env->CallStaticObjectMethod(
                inetAddressClass, inetAddress_getByAddress, buff);
        }
        break;
        case AF_INET:
        {
            sockaddr_in *ipv4 = (sockaddr_in*)&addr;
            jbyteArray buff = env->NewByteArray(4);
            if(buff == NULL)
            {
                LOGE("Error creating IPV4 byte array");
                return NULL;
            }

            env->SetByteArrayRegion(buff, 0, 4, (jbyte*)&ipv4->sin_addr);
            inetAddressObj = env->CallStaticObjectMethod(
                inetAddressClass, inetAddress_getByAddress, buff);
        }
        break;
    }
    if(env->ExceptionCheck() || inetAddressObj == NULL) {
        LOGE("Error creating InetAddress object");
        return NULL;
    }

    return inetAddressObj;
}

jobject newInetSocketAddress(JNIEnv *env, const sockaddr_storage &addr)
{
    LOGV("newInetSocketAddress Called");
    jclass inetSocketAddressClass = NULL;
    jmethodID inetSocketAddress_constructor = NULL;

    inetSocketAddressClass = cache.findClass("java/net/InetSocketAddress");
    if(env->ExceptionCheck() || inetSocketAddressClass == NULL)
    {
        LOGE("Error finding InetSocketAddress Class");
        return NULL;
    }

    jobject inetAddressObject = newInetAddress(env, addr);

    if(env->ExceptionCheck() || inetAddressObject == NULL)
    {
        LOGE("Error creating new inet address");
        return NULL;
    }

    inetSocketAddress_constructor = cache.findMethod(
        inetSocketAddressClass, "<init>", "(Ljava/net/InetAddress;I)V");
    if(env->ExceptionCheck() || inetSocketAddress_constructor == NULL)
    {
        LOGE("Error finding InetSocketAddress constructor");
        return NULL;
    }

    int port = 0;
    switch(addr.ss_family)
    {
        case AF_INET6:
        {
            LOGD("IPV6 Address");
            sockaddr_in6 *ipv6 = (sockaddr_in6*)&addr;
            port = ntohs(ipv6->sin6_port);
            LOGD("Port %d", port);
        }
        break;
        case AF_INET:
        {
            LOGD("IPV4 Address");
            sockaddr_in *ipv4 = (sockaddr_in*)&addr;
            port = ntohs(ipv4->sin_port);
            LOGD("Port: %d", port);
        }
        break;
        default:
        {
            LOGE("ERROR:  addr.ss_family is not set or unknown");
            break;
        }
    };


    jobject inetSocketAddressObject = env->NewObject(inetSocketAddressClass, inetSocketAddress_constructor, inetAddressObject, port);
    if(env->ExceptionCheck() || inetSocketAddressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
    }
    return inetSocketAddressObject;
}

jobject newMulticastGroup(JNIEnv *env, const ZT1_MulticastGroup &mc)
{
    jclass multicastGroupClass = NULL;
    jmethodID multicastGroup_constructor = NULL;

    jfieldID macField = NULL;
    jfieldID adiField = NULL;

    multicastGroupClass = cache.findClass("com/zerotier/sdk/MulticastGroup");
    if(env->ExceptionCheck() || multicastGroupClass == NULL)
    {
        return NULL;
    }

    multicastGroup_constructor = cache.findMethod(
        multicastGroupClass, "<init>", "()V");
    if(env->ExceptionCheck() || multicastGroup_constructor == NULL)
    {
        return NULL;
    }

    jobject multicastGroupObj = env->NewObject(multicastGroupClass, multicastGroup_constructor);
    if(env->ExceptionCheck() || multicastGroupObj == NULL)
    {
        return NULL;
    }

    macField = cache.findField(multicastGroupClass, "mac", "J");
    if(env->ExceptionCheck() || macField == NULL)
    {
        return NULL;
    }

    adiField = cache.findField(multicastGroupClass, "adi", "J");
    if(env->ExceptionCheck() || adiField == NULL)
    {
        return NULL;
    }

    env->SetLongField(multicastGroupObj, macField, mc.mac);
    env->SetLongField(multicastGroupObj, adiField, mc.adi);

    return multicastGroupObj;
}

jobject newPeerPhysicalPath(JNIEnv *env, const ZT1_PeerPhysicalPath &ppp)
{
    LOGV("newPeerPhysicalPath Called");
    jclass pppClass = NULL;

    jfieldID addressField = NULL;
    jfieldID lastSendField = NULL;
    jfieldID lastReceiveField = NULL;
    jfieldID fixedField = NULL;
    jfieldID activeField = NULL;
    jfieldID preferredField = NULL;

    jmethodID ppp_constructor = NULL;

    pppClass = cache.findClass("com/zerotier/sdk/PeerPhysicalPath");
    if(env->ExceptionCheck() || pppClass == NULL)
    {
        LOGE("Error finding PeerPhysicalPath class");
        return NULL;
    }

    addressField = cache.findField(pppClass, "address", "Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || addressField == NULL)
    {
        LOGE("Error finding address field");
        return NULL;
    }

    lastSendField = cache.findField(pppClass, "lastSend", "J");
    if(env->ExceptionCheck() || lastSendField == NULL)
    {
        LOGE("Error finding lastSend field");
        return NULL;
    }

    lastReceiveField = cache.findField(pppClass, "lastReceive", "J");
    if(env->ExceptionCheck() || lastReceiveField == NULL)
    {
        LOGE("Error finding lastReceive field");
        return NULL;
    }

    fixedField = cache.findField(pppClass, "fixed", "Z");
    if(env->ExceptionCheck() || fixedField == NULL)
    {
        LOGE("Error finding fixed field");
        return NULL;
    }

    activeField = cache.findField(pppClass, "active", "Z");
    if(env->ExceptionCheck() || activeField == NULL)
    {
        LOGE("Error finding active field");
        return NULL;
    }

    preferredField = cache.findField(pppClass, "preferred", "Z");
    if(env->ExceptionCheck() || preferredField == NULL)
    {
        LOGE("Error finding preferred field");
        return NULL;
    }

    ppp_constructor = cache.findMethod(pppClass, "<init>", "()V");
    if(env->ExceptionCheck() || ppp_constructor == NULL)
    {
        LOGE("Error finding PeerPhysicalPath constructor");
        return NULL;
    }

    jobject pppObject = env->NewObject(pppClass, ppp_constructor);
    if(env->ExceptionCheck() || pppObject == NULL)
    {
        LOGE("Error creating PPP object");
        return NULL; // out of memory
    }

    jobject addressObject = newInetSocketAddress(env, ppp.address);
    if(env->ExceptionCheck() || addressObject == NULL) {
        LOGE("Error creating InetSocketAddress object");
        return NULL;
    }

    env->SetObjectField(pppObject, addressField, addressObject);
    env->SetLongField(pppObject, lastSendField, ppp.lastSend);
    env->SetLongField(pppObject, lastReceiveField, ppp.lastReceive);
    env->SetBooleanField(pppObject, fixedField, ppp.fixed);
    env->SetBooleanField(pppObject, activeField, ppp.active);
    env->SetBooleanField(pppObject, preferredField, ppp.preferred);

    if(env->ExceptionCheck()) {
        LOGE("Exception assigning fields to PeerPhysicalPath object");
    }

    return pppObject;
}

jobject newPeer(JNIEnv *env, const ZT1_Peer &peer) 
{
    LOGV("newPeer called");

    jclass peerClass = NULL;

    jfieldID addressField = NULL;
    jfieldID lastUnicastFrameField = NULL;
    jfieldID lastMulticastFrameField = NULL;
    jfieldID versionMajorField = NULL;
    jfieldID versionMinorField = NULL;
    jfieldID versionRevField = NULL;
    jfieldID latencyField = NULL;
    jfieldID roleField = NULL;
    jfieldID pathsField = NULL;

    jmethodID peer_constructor = NULL;

    peerClass = cache.findClass("com/zerotier/sdk/Peer");
    if(env->ExceptionCheck() || peerClass == NULL)
    {
        LOGE("Error finding Peer class");
        return NULL;
    }

    addressField = cache.findField(peerClass, "address", "J");
    if(env->ExceptionCheck() || addressField == NULL)
    {
        LOGE("Error finding address field of Peer object");
        return NULL;
    }

    lastUnicastFrameField = cache.findField(peerClass, "lastUnicastFrame", "J");
    if(env->ExceptionCheck() || lastUnicastFrameField == NULL)
    {
        LOGE("Error finding lastUnicastFrame field of Peer object");
        return NULL;
    }

    lastMulticastFrameField = cache.findField(peerClass, "lastMulticastFrame", "J");
    if(env->ExceptionCheck() || lastMulticastFrameField == NULL)
    {
        LOGE("Error finding lastMulticastFrame field of Peer object");
        return NULL;
    }

    versionMajorField = cache.findField(peerClass, "versionMajor", "I");
    if(env->ExceptionCheck() || versionMajorField == NULL)
    {
        LOGE("Error finding versionMajor field of Peer object");
        return NULL;
    }

    versionMinorField = cache.findField(peerClass, "versionMinor", "I");
    if(env->ExceptionCheck() || versionMinorField == NULL)
    {
        LOGE("Error finding versionMinor field of Peer object");
        return NULL;
    }

    versionRevField = cache.findField(peerClass, "versionRev", "I");
    if(env->ExceptionCheck() || versionRevField == NULL)
    {
        LOGE("Error finding versionRev field of Peer object");
        return NULL;
    }

    latencyField = cache.findField(peerClass, "latency", "I");
    if(env->ExceptionCheck() || latencyField == NULL)
    {
        LOGE("Error finding latency field of Peer object");
        return NULL;
    }

    roleField = cache.findField(peerClass, "role", "Lcom/zerotier/sdk/PeerRole;");
    if(env->ExceptionCheck() || roleField == NULL)
    {
        LOGE("Error finding role field of Peer object");
        return NULL;
    }

    pathsField = cache.findField(peerClass, "paths", "[Lcom/zerotier/sdk/PeerPhysicalPath;");
    if(env->ExceptionCheck() || pathsField == NULL)
    {
        LOGE("Error finding paths field of Peer object");
        return NULL;
    }

    peer_constructor = cache.findMethod(peerClass, "<init>", "()V");
    if(env->ExceptionCheck() || peer_constructor == NULL)
    {
        LOGE("Error finding Peer constructor");
        return NULL;
    }

    jobject peerObject = env->NewObject(peerClass, peer_constructor);
    if(env->ExceptionCheck() || peerObject == NULL)
    {
        LOGE("Error creating Peer object");
        return NULL; // out of memory
    }

    env->SetLongField(peerObject, addressField, (jlong)peer.address);
    env->SetLongField(peerObject, lastUnicastFrameField, (jlong)peer.lastUnicastFrame);
    env->SetLongField(peerObject, lastMulticastFrameField, (jlong)peer.lastMulticastFrame);
    env->SetIntField(peerObject, versionMajorField, peer.versionMajor);
    env->SetIntField(peerObject, versionMinorField, peer.versionMinor);
    env->SetIntField(peerObject, versionRevField, peer.versionRev);
    env->SetIntField(peerObject, latencyField, peer.latency);
    env->SetObjectField(peerObject, roleField, createPeerRole(env, peer.role));

    jclass peerPhysicalPathClass = cache.findClass("com/zerotier/sdk/PeerPhysicalPath");
    if(env->ExceptionCheck() || peerPhysicalPathClass == NULL)
    {
        LOGE("Error finding PeerPhysicalPath class");
        return NULL;
    }

    jobjectArray arrayObject = env->NewObjectArray(
        peer.pathCount, peerPhysicalPathClass, NULL);
    if(env->ExceptionCheck() || arrayObject == NULL) 
    {
        LOGE("Error creating PeerPhysicalPath[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < peer.pathCount; ++i)
    {
        jobject path = newPeerPhysicalPath(env, peer.paths[i]);

        env->SetObjectArrayElement(arrayObject, i, path);
        if(env->ExceptionCheck()) {
            LOGE("exception assigning PeerPhysicalPath to array");
            break;
        }
    }

    env->SetObjectField(peerObject, pathsField, arrayObject);

    return peerObject;
}

jobject newNetworkConfig(JNIEnv *env, const ZT1_VirtualNetworkConfig &vnetConfig)
{
    jclass vnetConfigClass = NULL;
    jmethodID vnetConfig_constructor = NULL;
    jfieldID nwidField = NULL;
    jfieldID macField = NULL;
    jfieldID nameField = NULL;
    jfieldID statusField = NULL;
    jfieldID typeField = NULL;
    jfieldID mtuField = NULL;
    jfieldID dhcpField = NULL;
    jfieldID bridgeField = NULL;
    jfieldID broadcastEnabledField = NULL;
    jfieldID portErrorField = NULL;
    jfieldID enabledField = NULL;
    jfieldID netconfRevisionField = NULL;
    jfieldID multicastSubscriptionsField = NULL;
    jfieldID assignedAddressesField = NULL;

    vnetConfigClass = cache.findClass("com/zerotier/sdk/VirtualNetworkConfig");
    if(vnetConfigClass == NULL)
    {
        LOGE("Couldn't find com.zerotier.sdk.VirtualNetworkConfig");
        return NULL;
    }

    vnetConfig_constructor = cache.findMethod(
        vnetConfigClass, "<init>", "()V");
    if(env->ExceptionCheck() || vnetConfig_constructor == NULL)
    {
        LOGE("Couldn't find VirtualNetworkConfig Constructor");
        return NULL;
    }

    jobject vnetConfigObj = env->NewObject(vnetConfigClass, vnetConfig_constructor);
    if(env->ExceptionCheck() || vnetConfigObj == NULL)
    {
        LOGE("Error creating new VirtualNetworkConfig object");
        return NULL;
    }

    nwidField = cache.findField(vnetConfigClass, "nwid", "J");
    if(env->ExceptionCheck() || nwidField == NULL)
    {
        LOGE("Error getting nwid field");
        return NULL;
    }

    macField = cache.findField(vnetConfigClass, "mac", "J");
    if(env->ExceptionCheck() || macField == NULL)
    {
        LOGE("Error getting mac field");
        return NULL;
    }

    nameField = cache.findField(vnetConfigClass, "name", "Ljava/lang/String;");
    if(env->ExceptionCheck() || nameField == NULL)
    {
        LOGE("Error getting name field");
        return NULL;
    }

    statusField = cache.findField(vnetConfigClass, "status", "Lcom/zerotier/sdk/VirtualNetworkStatus;");
    if(env->ExceptionCheck() || statusField == NULL)
    {
        LOGE("Error getting status field");
        return NULL;
    }

    typeField = cache.findField(vnetConfigClass, "type", "Lcom/zerotier/sdk/VirtualNetworkType;");
    if(env->ExceptionCheck() || typeField == NULL)
    {
        LOGE("Error getting type field");
        return NULL;
    }

    mtuField = cache.findField(vnetConfigClass, "mtu", "I");
    if(env->ExceptionCheck() || mtuField == NULL)
    {
        LOGE("Error getting mtu field");
        return NULL;
    }

    dhcpField = cache.findField(vnetConfigClass, "dhcp", "Z");
    if(env->ExceptionCheck() || dhcpField == NULL)
    {
        LOGE("Error getting dhcp field");
        return NULL;
    }

    bridgeField = cache.findField(vnetConfigClass, "bridge", "Z");
    if(env->ExceptionCheck() || bridgeField == NULL)
    {
        LOGE("Error getting bridge field");
        return NULL;
    }

    broadcastEnabledField = cache.findField(vnetConfigClass, "broadcastEnabled", "Z");
    if(env->ExceptionCheck() || broadcastEnabledField == NULL)
    {
        LOGE("Error getting broadcastEnabled field");
        return NULL;
    }

    portErrorField = cache.findField(vnetConfigClass, "portError", "I");
    if(env->ExceptionCheck() || portErrorField == NULL)
    {
        LOGE("Error getting portError field");
        return NULL;
    }

    enabledField = cache.findField(vnetConfigClass, "enabled", "Z");
    if(env->ExceptionCheck() || enabledField == NULL)
    {
        LOGE("Error getting enabled field");
        return NULL;
    }

    netconfRevisionField = cache.findField(vnetConfigClass, "netconfRevision", "J");
    if(env->ExceptionCheck() || netconfRevisionField == NULL)
    {
        LOGE("Error getting netconfRevision field");
        return NULL;
    }

    multicastSubscriptionsField = cache.findField(vnetConfigClass, "multicastSubscriptions", "[Lcom/zerotier/sdk/MulticastGroup;");
    if(env->ExceptionCheck() || multicastSubscriptionsField == NULL)
    {
        LOGE("Error getting multicastSubscriptions field");
        return NULL;
    }

    assignedAddressesField = cache.findField(vnetConfigClass, "assignedAddresses", "[Ljava/net/InetSocketAddress;");
    if(env->ExceptionCheck() || assignedAddressesField == NULL)
    {
        LOGE("Error getting assignedAddresses field");
        return NULL;
    }

    env->SetLongField(vnetConfigObj, nwidField, vnetConfig.nwid);
    env->SetLongField(vnetConfigObj, macField, vnetConfig.mac);
    jstring nameStr = env->NewStringUTF(vnetConfig.name);
    if(env->ExceptionCheck() || nameStr == NULL)
    {
        return NULL; // out of memory
    }
    env->SetObjectField(vnetConfigObj, nameField, nameStr);

    jobject statusObject = createVirtualNetworkStatus(env, vnetConfig.status);
    if(env->ExceptionCheck() || statusObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, statusField, statusObject);

    jobject typeObject = createVirtualNetworkType(env, vnetConfig.type);
    if(env->ExceptionCheck() || typeObject == NULL)
    {
        return NULL;
    }
    env->SetObjectField(vnetConfigObj, typeField, typeObject);

    env->SetIntField(vnetConfigObj, mtuField, vnetConfig.mtu);
    env->SetBooleanField(vnetConfigObj, dhcpField, vnetConfig.dhcp);
    env->SetBooleanField(vnetConfigObj, bridgeField, vnetConfig.bridge);
    env->SetBooleanField(vnetConfigObj, broadcastEnabledField, vnetConfig.broadcastEnabled);
    env->SetIntField(vnetConfigObj, portErrorField, vnetConfig.portError);

    jclass multicastGroupClass = cache.findClass("com/zerotier/sdk/MulticastGroup");
    if(env->ExceptionCheck() || multicastGroupClass == NULL) 
    {
        LOGE("Error finding MulticastGroup class");
        return NULL;
    }

    jobjectArray mcastSubsArrayObj = env->NewObjectArray(
        vnetConfig.multicastSubscriptionCount, multicastGroupClass, NULL);
    if(env->ExceptionCheck() || mcastSubsArrayObj == NULL) {
        LOGE("Error creating MulticastGroup[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < vnetConfig.multicastSubscriptionCount; ++i)
    {
        jobject mcastObj = newMulticastGroup(env, vnetConfig.multicastSubscriptions[i]);
        env->SetObjectArrayElement(mcastSubsArrayObj, i, mcastObj);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning MulticastGroup to array");
        }
    }
    env->SetObjectField(vnetConfigObj, multicastSubscriptionsField, mcastSubsArrayObj);

    jclass inetSocketAddressClass = cache.findClass("java/net/InetSocketAddress");
    if(env->ExceptionCheck() || inetSocketAddressClass == NULL)
    {
        LOGE("Error finding InetSocketAddress class");
        return NULL;
    }

    jobjectArray assignedAddrArrayObj = env->NewObjectArray(
        vnetConfig.assignedAddressCount, inetSocketAddressClass, NULL);
    if(env->ExceptionCheck() || assignedAddrArrayObj == NULL)
    {
        LOGE("Error creating InetSocketAddress[] array");
        return NULL;
    }

    for(unsigned int i = 0; i < vnetConfig.assignedAddressCount; ++i)
    {
        jobject inetAddrObj = newInetSocketAddress(env, vnetConfig.assignedAddresses[i]);
        env->SetObjectArrayElement(assignedAddrArrayObj, i, inetAddrObj);
        if(env->ExceptionCheck())
        {
            LOGE("Error assigning InetSocketAddress to array");
            return NULL;
        }
    }

    env->SetObjectField(vnetConfigObj, assignedAddressesField, assignedAddrArrayObj);

    return vnetConfigObj;
}

jobject newVersion(JNIEnv *env, int major, int minor, int rev, long featureFlags)
{
   // create a com.zerotier.sdk.Version object
    jclass versionClass = NULL;
    jmethodID versionConstructor = NULL;

    versionClass = cache.findClass("com/zerotier/sdk/Version");
    if(env->ExceptionCheck() || versionClass == NULL)
    {
        return NULL;
    }

    versionConstructor = cache.findMethod(
        versionClass, "<init>", "()V");
    if(env->ExceptionCheck() || versionConstructor == NULL)
    {
        return NULL;
    }

    jobject versionObj = env->NewObject(versionClass, versionConstructor);
    if(env->ExceptionCheck() || versionObj == NULL)
    {
        return NULL;
    }

    // copy data to Version object
    jfieldID majorField = NULL;
    jfieldID minorField = NULL;
    jfieldID revisionField = NULL;
    jfieldID featureFlagsField = NULL;

    majorField = cache.findField(versionClass, "major", "I");
    if(env->ExceptionCheck() || majorField == NULL)
    {
        return NULL;
    }

    minorField = cache.findField(versionClass, "minor", "I");
    if(env->ExceptionCheck() || minorField == NULL)
    {
        return NULL;
    }

    revisionField = cache.findField(versionClass, "revision", "I");
    if(env->ExceptionCheck() || revisionField == NULL)
    {
        return NULL;
    }

    featureFlagsField = cache.findField(versionClass, "featureFlags", "J");
    if(env->ExceptionCheck() || featureFlagsField == NULL)
    {
        return NULL;
    }

    env->SetIntField(versionObj, majorField, (jint)major);
    env->SetIntField(versionObj, minorField, (jint)minor);
    env->SetIntField(versionObj, revisionField, (jint)rev);
    env->SetLongField(versionObj, featureFlagsField, (jlong)featureFlags); 

    return versionObj;
}

#ifdef __cplusplus
}
#endif