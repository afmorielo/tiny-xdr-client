#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include "mpmessages.hxx"
#include <sys/time.h>
#include <thread>
#include "tiny_xdr.hxx"
#include <unistd.h>

class XDRClient {
public:
    XDRClient(const char* serverIp, int serverPort) {

        // Attemps to create a UDP socket
        serverSocket = socket(AF_INET, SOCK_DGRAM, 0);

        if (serverSocket == -1) {
            std::cerr << "[ERROR] Failed to create socket at " << __FUNCTION__ << ": " << strerror(errno) << " (" << errno << ") " << "\n";
            std::exit(1);
        }

        // Set up the server address structure
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(serverPort);

        if (inet_pton(AF_INET, serverIp, &serverAddress.sin_addr) <= 0) {
            std::cerr << "[ERROR] Invalid address or address not supported at " << __FUNCTION__ << ": " << strerror(errno) << " (" << errno << ") " << "\n";
            close(serverSocket);
            std::exit(1);
        }

    }

    ~XDRClient() {

        // Close the socket if it was opened
        if (serverSocket != -1) {
            close(serverSocket);
        }
        
    }

    void send(
        const char* model,
        const double currentPosition[3],
        const float currentOrientation[3],
        const float linearVel[3],
        const float angularVel[3],
        const float linearAccel[3],
        const float angularAccel[3],
        const char* callsign
    ) {
        std::cout << "Sending data...\n";

        // Prepare and send the message
        T_PositionMsg positionMsg;
        populatePositionMsg(
            positionMsg,
            model,
            currentPosition,
            currentOrientation,
            linearVel,
            angularVel,
            linearAccel,
            angularAccel,
            1.0,  // lag
            0.0   // pad
        );

        T_MsgHdr msgHdr;
        populateMsgHdr(
            msgHdr,
            MSG_MAGIC,
            PROTO_VER,
            POS_DATA_ID,
            sizeof(T_MsgHdr) + sizeof(T_PositionMsg),
            0,
            100,
            0,
            callsign
        );

        // Combine the header and message into a single buffer
        char buffer[sizeof(msgHdr) + sizeof(positionMsg)];
        std::memcpy(buffer, &msgHdr, sizeof(msgHdr));
        std::memcpy(buffer + sizeof(msgHdr), &positionMsg, sizeof(positionMsg));

        // Send the buffer to the server using sendto() for UDP
        if (sendto(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
            std::cerr << "[ERROR] Failed to send data at " << __FUNCTION__ << ": " << strerror(errno) << " (" << errno << ") " << "\n";
        }

    }

    void receive() {

        while (true) {

            std::cout << "Receiving data...\n";

            T_MsgHdr msgHdr;
            T_PositionMsg positionMsg;

            char buffer[1024]; // Adjust the buffer size accordingly
            socklen_t receiveAddrLen = sizeof(serverAddress);
            ssize_t bytesReceived = recvfrom(serverSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddress, &receiveAddrLen);

            if (bytesReceived == -1) {
                std::cerr << "[ERROR] Failed to receive data: " << strerror(errno) << " (" << errno << ")\n";
            } else if (bytesReceived == 0) {
                std::cerr << "[ERROR] Connection closed by server!\n";
                break;
            } else {

                // Assuming sizeof(T_MsgHdr) is the size of the header in the buffer
                std::memcpy(&msgHdr, buffer, sizeof(T_MsgHdr));
                // Assuming sizeof(T_MsgHdr) is the size of the header in the buffer
                std::memcpy(&positionMsg, buffer + sizeof(T_MsgHdr), sizeof(T_PositionMsg));

                // Decode specific values from the received message
                uint32_t magic = XDR_decode_uint32(msgHdr.Magic);
                uint32_t version = XDR_decode_uint32(msgHdr.Version);
                // ... Decode other fields as needed

                std::cout << "Received data:\n";
                std::cout << magic << ",";
                std::cout << version << ",";
                std::cout << msgHdr.Callsign << ",";
                for (unsigned i = 0;  i < 3; ++i) {
                    std::cout << XDR_decode_double(positionMsg.position[i]) << ",";
                }
                std::cout << "\n";
                // ... Print other decoded values

                // Now you have the decoded values in the msgHdr and positionMsg structures.
                // You can use these values as needed for further processing.
            }
        }
    }

private:
    int serverSocket;
    struct sockaddr_in serverAddress;
    const char* serverIp;
    int serverPort;

    void populatePositionMsg(
        T_PositionMsg &positionMsg,
        const char* model,
        const double currentPosition[3],
        const float currentOrientation[3],
        const float linearVel[3],
        const float angularVel[3],
        const float linearAccel[3],
        const float angularAccel[3],
        double lag,
        double pad
    ) {
        strncpy(positionMsg.Model, model, MAX_MODEL_NAME_LEN - 1);
        positionMsg.Model[MAX_MODEL_NAME_LEN - 1] = '\0';
        positionMsg.time = XDR_encode_double(static_cast<double>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()));
        positionMsg.lag = XDR_encode_double(lag);

        for (unsigned i = 0; i < 3; ++i) {
            positionMsg.position[i] = XDR_encode_double(currentPosition[i]);
            positionMsg.orientation[i] = XDR_encode_float(currentOrientation[i]);
            positionMsg.linearVel[i] = XDR_encode_float(linearVel[i]);
            positionMsg.angularVel[i] = XDR_encode_float(angularVel[i]);
            positionMsg.linearAccel[i] = XDR_encode_float(linearAccel[i]);
            positionMsg.angularAccel[i] = XDR_encode_float(angularAccel[i]);
        }

        positionMsg.pad = XDR_encode_double(pad);
    }

    void populateMsgHdr(
        T_MsgHdr &msgHdr,
        uint32_t magic,
        uint32_t version,
        uint32_t msgId,
        uint32_t msgLen,
        uint32_t requestedRangeNmMin,
        uint32_t requestedRangeNmMax,
        uint16_t replyPort,
        const char* callsign
    ) {
        // Populate msgHdr fields as needed
        // Writing the Header (some fixed or default values)
        msgHdr.Magic = XDR_encode_uint32(magic);
        msgHdr.Version = XDR_encode_uint32(version);
        msgHdr.MsgId = XDR_encode_uint32(msgId);
        msgHdr.MsgLen = XDR_encode_uint32(msgLen);
        msgHdr.RequestedRangeNm = XDR_encode_shortints32(requestedRangeNmMin, requestedRangeNmMax);
        msgHdr.ReplyPort = XDR_encode_uint16(replyPort);
        strncpy(msgHdr.Callsign, callsign, MAX_CALLSIGN_LEN);
        msgHdr.Callsign[MAX_CALLSIGN_LEN - 1] = '\0';
    }
};

int main() {
    XDRClient client("127.0.0.1", 5000);

    // Create a thread for the receive() method
    std::thread receiveThread(&XDRClient::receive, &client);

    // Main loop for sending data
    while (true) {
        client.send("Aircraft/f16/Models/F-16.xml",
                    std::array<double, 3>{2589114.800074, -1080806.835734, 5708738.990279}.data(),
                    std::array<float, 3>{-2.130530, -1.660662, 0.242749}.data(),
                    std::array<float, 3>{0.0, 0.0, 0.0}.data(),
                    std::array<float, 3>{0.0, 0.0, 0.0}.data(),
                    std::array<float, 3>{0.0, 0.0, 0.0}.data(),
                    std::array<float, 3>{0.0, 0.0, 0.0}.data(),
                    "AFMC");

        // Add a delay if needed to control the sending rate
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 Hz delay
    }

    // This line will never be reached, but it's here for completeness
    receiveThread.join();

    return 0;
}