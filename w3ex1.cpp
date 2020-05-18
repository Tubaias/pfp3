
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>

typedef uint64_t u64;
typedef uint8_t u8;

using namespace std;

class VbyteArray {
private:
	u8* arr;
	u64 size;

public:
	VbyteArray(u64 n, u64* memblock) {
		size = n;
		arr = compress(memblock);
	}

	u8* compress(u64* memblock) {
		// compress the integers with vbyte
		u8* temp = new u8[size];
		u64 bytes = 0;

		for (u64 i = 0; i < size / 8; i++) {
			u64 num = memblock[i];

			while (true) {
				// same as u8 byte = num % 128;
				u8 byte = num & 0x7F;

				if (num < 128) {
					temp[bytes] = byte + 128;
					bytes++;
					break;
				}

				temp[bytes] = byte;
				bytes++;
				// same as num /= 128;
				num = num >> 7;
			}
		}

		// copy bytes to an appropriately sized array and delete the old one
		size = bytes;
		u8* compressed = new u8[size];

		for (int i = 0; i < size; i++) {
			compressed[i] = temp[i];
		}

		delete[] temp;
		return compressed;
	}

	u64 accessScan(u64 i) {
		// find position of desired integer in the compressed array
		u64 startPos = 0;

		u64 pos = 0;
		for (int j = 0; j < size; j++) {
			if (pos == i) {
				startPos = j;
				break;
			}

			if (arr[j] > 127) {
				pos++;
			}
		}

		// read the decompressed value of desired integer
		u64 num = 0;
		u64 bytes = 0;
		for (int k = startPos; k < size; k++) {
			u8 byte = arr[k];
			// converted to u64 to avoid overflows on the left shift
			u64 byte64 = byte;

			if (byte > 127) {
				num += (byte64 - 128) << (7 * bytes);
				return num;
			} else {
				num += byte64 << (7 * bytes);
				bytes++;
			}
		}

		return 0;
	}

	u64 getSizeInBytes() {
		return size + 8;
	}

	void dealloc() {
		delete[] arr;
	}
};

void benchmark(u64 size, VbyteArray a, u64 n) {
	vector<u64> ints;
	mt19937 g(random_device{}());
	uniform_int_distribution<u64> d(0, (size / 8) - 1);

	for (u64 i = 0; i < n; i++) {
		ints.push_back(d(g));
	}

	// system_clock used instead of high_resolution_clock due to visual studio compatibility
	auto begin = chrono::system_clock::now();

	for (int i : ints) {
		a.accessScan(i);
	}

	chrono::duration<double> time = chrono::system_clock::now() - begin;
	cout << n << " accessScan operations took: " << time.count() << "s\n";
}

int main(int argc, char** argv) {
	if (argc < 3) {
		cout << "\ninsufficient arguments.\n";
		return 1;
	}

	string filename = argv[1];
	u64 ops = atoi(argv[2]);

	// read the file
	ifstream in(filename, ios::binary | ios::ate);

	if (!in.is_open()) {
		cout << "file " << filename << " could not be opened.\n";
		return 1;
	}

	const streampos size = in.tellg();
	u64* memblock = new u64[size / 8];
	in.seekg(0, ios::beg);
	in.read(reinterpret_cast<char*> (memblock), size);
	in.close();

	// initialize the array
	u64 size64 = size;
	VbyteArray a(size64, memblock);

	// testing
	cout << "class instance size: " << a.getSizeInBytes() << " bytes (" << a.getSizeInBytes() / 1048576 << "MB)\n";
	benchmark(size64, a, ops);

	delete[] memblock;
	a.dealloc();
	return 0;
}
