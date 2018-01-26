#ifndef INCLUDE_P2PMESSAGE_HPP_
#define INCLUDE_P2PMESSAGE_HPP_

#include "MessageType.hpp"
#include "stdint.h"



/// Class describing each message in the network.
class P2PMessage {
public:
	uint32_t getAdditionalDataSize() const {
		return additionalDataSize;
	}

	void setAdditionalDataSize(uint32_t additionalDataSize) {
		this->additionalDataSize = additionalDataSize;
	}

	MessageType getMessageType() const {
		return messageType;
	}

	void setMessageType(MessageType messageType) {
		this->messageType = messageType;
	}

private:
	MessageType messageType;
	uint32_t additionalDataSize;
};


#endif /* INCLUDE_P2PMESSAGE_HPP_ */
