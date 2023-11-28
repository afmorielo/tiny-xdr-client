#include <iostream>
#include <chrono>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <thread>
#include "mpmessages.hxx"
#include "tiny_xdr.hxx"
#include <sys/time.h>

const char *FGMS_SERVER_IP = "127.0.0.1";        // Replace with your FGMS server IP
const int FGMS_SERVER_PORT = 5000;               // Replace with your FGMS server port

class XDRClient {
public:
    XDRClient() {
        clientSocket = -1;
        initializeSocket(clientSocket, FGMS_SERVER_PORT);
    }

    ~XDRClient() {
        // Close the socket if it was opened
        if (clientSocket != -1) {
            close(clientSocket);
        }
    }

    void initializeSocket(int& sock, int port) {

        // Create a UDP socket
        sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock == -1) {
            std::cerr << "Error creating socket: " << strerror(errno) << " (" << errno << ")\n";
            std::exit(1);
        }

        // Set up the server address structure
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(FGMS_SERVER_PORT);
        if (inet_pton(AF_INET, FGMS_SERVER_IP, &serverAddress.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported\n";
            close(sock);
            std::exit(1);
        }

    }

    void sendData() {

        while (true) {
            std::cout << "Sending data...\n";
            // Get the new position
            double currentPosition[3] = {2589114.800074, -1080806.835734, 5708738.990279};
            double currentOrientation[3] = {-2.130530, -1.660662, 0.242749};

            // Prepare and send the message
            T_PositionMsg positionMsg;
            populatePositionMsg(positionMsg, currentPosition, currentOrientation);

            T_MsgHdr msgHdr;
            populateMsgHdr(msgHdr);

            // Combine the header and message into a single buffer
            char buffer[sizeof(msgHdr) + sizeof(positionMsg)];
            std::memcpy(buffer, &msgHdr, sizeof(msgHdr));
            std::memcpy(buffer + sizeof(msgHdr), &positionMsg, sizeof(positionMsg));

            // Send the buffer to the server using sendto() for UDP
            if (sendto(clientSocket, buffer, sizeof(buffer), 0, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) == -1) {
                std::cerr << "Error sending data: " << strerror(errno) << " (" << errno << ")\n";
            }

            // Add a delay if needed to control the sending rate
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10 Hz delay
        }
    }

    void receiveData() {

        while (true) {
            std::cout << "Receiving data...\n";
            char buffer[1024]; // Adjust the buffer size accordingly
            socklen_t receiveAddrLen = sizeof(serverAddress);
            ssize_t bytesReceived = recvfrom(clientSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&serverAddress, &receiveAddrLen);

            if (bytesReceived == -1) {
                std::cerr << "Error receiving data: " << strerror(errno) << " (" << errno << ")\n";
            } else if (bytesReceived == 0) {
                std::cerr << "Connection closed by server\n";
                break;
            } else {
                // Process the received data as needed
                // TODO: Implement data processing logic

                std::cout << "Received data: " << buffer << std::endl;
            }
        }
    }

private:
    int clientSocket;
    struct sockaddr_in serverAddress;

    void populatePositionMsg(T_PositionMsg &positionMsg, const double currentPosition[3], const double currentOrientation[3]) {
        strncpy(positionMsg.Model, "Aircraft/f16/Models/F-16.xml", MAX_MODEL_NAME_LEN - 1);
        positionMsg.Model[MAX_MODEL_NAME_LEN - 1] = '\0';
        positionMsg.time = XDR_encode_double(static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::system_clock::now().time_since_epoch())
                .count()));
        positionMsg.lag = XDR_encode_double(1.0);

        for (unsigned i = 0;  i < 3; ++i) {
            positionMsg.position[i] = XDR_encode_double(currentPosition[i]);
            positionMsg.orientation[i] = XDR_encode_float(currentOrientation[i]);
            positionMsg.linearVel[i] = XDR_encode_float(0.0);
            positionMsg.angularVel[i] = XDR_encode_float(0.0);
            positionMsg.linearAccel[i] = XDR_encode_float(0.0);
            positionMsg.angularAccel[i] = XDR_encode_float(0.0);
        }

        positionMsg.pad = 0;
    }

    void populateMsgHdr(T_MsgHdr &msgHdr) {
        // Populate msgHdr fields as needed
        // Writing the Header (some fixed values)
        msgHdr.Magic = XDR_encode_uint32(MSG_MAGIC);
        msgHdr.Version = XDR_encode_uint32(PROTO_VER);
        msgHdr.MsgId = XDR_encode_uint32(POS_DATA_ID);
        msgHdr.MsgLen = XDR_encode_uint32((uint32_t) (sizeof(msgHdr) + sizeof(T_PositionMsg)));
        msgHdr.RequestedRangeNm = XDR_encode_shortints32(0, 100);
        msgHdr.ReplyPort = 0;
        strncpy(msgHdr.Callsign, "AFMC", MAX_CALLSIGN_LEN);
        msgHdr.Callsign[MAX_CALLSIGN_LEN - 1] = '\0';
    }
};

int main() {
    XDRClient client;

    // Create a thread for the sendData() method
    std::thread sendThread(&XDRClient::sendData, &client);

    // Create a thread for the receiveData() method
    std::thread receiveThread(&XDRClient::receiveData, &client);

    // Wait for both threads to finish
    sendThread.join();
    receiveThread.join();

    return 0;
}