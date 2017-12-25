#ifndef INCLUDE_PROTOCOLSTRUCTS_HPP_
#define INCLUDE_PROTOCOLSTRUCTS_HPP_

#include "md5hash.hpp"
#include <stdint.h>
#include <cstring>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>

const size_t MAX_FILENAME_LEN = 255;

enum class MessageType {
	// raportowanie stanu
	HELLO,				// UDP komunikat wysyłany przez nowoutworzony węzeł
	HELLO_REPLY,		// TCP odpowiedź od węzłów, które usłyszały HELLO. Dołącza tablicę deskryptorów plików, które znajdowały się w danej chwili w konkretnym
	DISCONNECTING,		// UDP powiadomienie sieci o rozpoczęciu odłączania się
	CONNECTION_LOST,	// UDP powiadomienie sieci o utraceniu węzła o określonym IP (podanym w sekcji danych)
	CMD_REFUSED,		// TCP powiadomienie węzła, który złożył żądanie (np. o pobranie pliku) o braku możliwości wykonania transkacji (np. dostęp do pliku oznaczonego jako "tymczasowo nieważny" albo próba przesłania pliku do węzła w stanie "disconnecting")

	// zarządzanie plikami
	NEW_FILE,			// UDP powiadomienie sieci o nowym pliku o danym deskryptorze (podanym w sekcji danych)
	REVOKE_FILE,		// UDP powiadomienie sieci o usunięciu pliku o danym deskryptorze (podanym w sekcji danych)
	DISCARD_DESCRIPTOR,	// UDP oznacz podany deskryptor jako "tymczasowo nieważny"
	UPDATE_DESCRIPTOR,	// UDP rozgłoś nową wersję deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny")
	HOLDER_CHANGE,		// TCP przesłanie do określonego węzła zaktualizowanego deskryptora (wcześniej oznaczonego jako "tymczasowo nieważny") oraz pliku
	FILE_TRANSFER,		// TCP przesłanie deskryptora oraz zawartości pliku do węzła, który wcześniej tego zażądał

	// interfejs użytkownika
	UPLOAD_FILE,		// TCP żądanie uploadu pliku, zawiera w sekcji danych: deskryptor oraz plik (jako tablica bajtów)
	GET_FILE,			// TCP żądanie przesłania pliku o danym deskryptorze (podanym w sekcji danych) od węzła przetrzymującego plik
	DELETE_FILE,		// TCP żądanie unieważnienia pliku o danym deskryptorze (podanym w sekcji danych) do węzła przetrzymującego plik
};

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

class FileDescriptor {
public:
	FileDescriptor(const std::string& filename) {
		setName(filename);
		setSize(obtainFileSize(name));
		setMd5(Md5sum(filename).getMd5Hash());
	}

	uint32_t getHolderIp() const {
		return holderIp;
	}

	void setHolderIp(uint32_t holderIp) {
		this->holderIp = holderIp;
	}

	const Md5Hash& getMd5() const {
		return md5;
	}

	void setMd5(const Md5Hash& md5) {
		this->md5 = md5;
	}

	std::string getName() const {
		return std::string(name);
	}

	void setName(const std::string& name) {
		if (name.size() > MAX_FILENAME_LEN) {
			throw std::invalid_argument("filename can't be longer than "
					+ std::to_string(MAX_FILENAME_LEN));
		}
		strcpy(this->name, name.c_str());
	}

	uint32_t getOwnerIp() const {
		return ownerIp;
	}

	void setOwnerIp(uint32_t ownerIp) {
		this->ownerIp = ownerIp;
	}

	uint32_t getSize() const {
		return size;
	}

	void setSize(uint32_t size) {
		this->size = size;
	}

	time_t getUploadTime() const {
		return uploadTime;
	}

	std::string getFormattedUploadTime() const {
		char *formattedTime = ctime(&uploadTime);
		return std::string(formattedTime);
	}

	void setUploadTime(time_t uploadTime) {
		this->uploadTime = uploadTime;
	}

private:
	static uint32_t obtainFileSize(const char* fn) {
		struct stat st;
		if (stat(fn, &st) == -1) {
			throw std::invalid_argument(std::string(fn) + " doesn't exist");
		}
		return st.st_size;
	}

	char name[MAX_FILENAME_LEN+1];
	Md5Hash md5;
	uint32_t size;
	time_t uploadTime;
	uint32_t ownerIp;
	uint32_t holderIp;
};

#endif /* INCLUDE_PROTOCOLSTRUCTS_HPP_ */
