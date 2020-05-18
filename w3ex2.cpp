
#include <iostream>
#include <fstream>
#include <vector>
#include <random>
#include <chrono>
#include <iomanip>
// for visual studio popcnt
//#include <intrin.h>

typedef uint64_t u64;
typedef uint8_t u8;

// visual studio
//#define popcnt __popcnt64
// gcc
#define popcnt __builtin_popcountll 

using namespace std;

// SumBitArray class from assignment 2
class SumBitArray {
private:
	u64* arr;
	u64* sums;
	u64 size;
	u64 suminterval;

public:
	SumBitArray(u64 n) {
		// size of bit array
		size = n / 64;
		if (n % 64 != 0) size++;
		arr = new u64[size]();

		sums = new u64[size]();
		suminterval = 1;
	}

	u64 get(u64 i) {
		// u64 containing value to get
		u64 num = arr[i / 64];
		// read the value with a bit mask and return it
		u64 mask = 1;
		mask <<= (i % 64);
		return ((num & mask) == 0) ? 0 : 1;
	}

	void set(u64 i, u64 b) {
		// u64 containing the position to write to
		u64 num = arr[i / 64];
		// set the bit to 0 or 1 with a bit mask
		u64 mask = 1;
		mask <<= (i % 64);

		if (b == 0) {
			num &= ~mask;
		}
		else {
			num |= mask;
		}

		// save the new u64 to the array
		arr[i / 64] = num;
	}

	// generating support strucure for sum function
	void generateSums() {
		u64 total = 0;

		for (u64 i = 0; i < size; i++) {
			// number of 1-bits in arr[i]
			total += popcnt(arr[i]);

			// if enough u64's have been summed, save current total
			if ((i + 1) % suminterval == 0) {
				sums[((i + 1) / suminterval) - 1] = total;
			}
		}
	}

	u64 sum(u64 i) {
		// helper values
		u64 bigdiv = (i + 1) / (suminterval * 64);
		u64 bigmod = (i + 1) % (suminterval * 64);
		u64 div = (i + 1) / 64;
		u64 mod = (i + 1) % 64;
		u64 sum = 0;

		// use support structure if range to sum is big enough
		if (bigdiv > 0) {
			sum = sums[bigdiv - 1];
		}

		// add remaining 1-bits to the sum
		if (bigmod != 0) {
			// whole u64's
			for (u64 j = (bigdiv * suminterval); j < div; j++) {
				u64 num = arr[j];
				sum += popcnt(num);
			}

			// last non-whole u64
			if (mod != 0) {
				u64 num = arr[div];
				u64 mask = 0;
				mask = (~mask) >> (64 - mod);
				sum += popcnt(num & mask);
			}
		}

		return sum;
	}

	u64 getSizeInBytes() {
		return size * 2 + 16;
	}

	void dealloc() {
		delete[] arr;
		delete[] sums;
	}
};

class LayeredVbyteArray {
private:
	u64 layers;
	u64* sizes;
	u8** datab;
	SumBitArray** stopb;

public:
	LayeredVbyteArray(u64 n, u64* memblock) {
		// initialize the internal structures
		initStructures(n, memblock);
		// fill them with vbyte values
		fillStructures(n, memblock);
		// generate sums for the BitArrays
		generateSums();
	}

	void initStructures(u64 n, u64* memblock) {
		// calculate number and sizes of layers
		u64 temp_sizes[9] = {0};
		layers = 0;

		for (int i = 0; i < n / 8; i++) {
			u64 num = memblock[i];

			for (int j = 0; j < 9; j++) {
				u8 byte = num & 0x7F;
				temp_sizes[j] += 1;

				if (num < 128) {
					if (j > layers) layers = j;

					break;
				}

				num = num >> 7;
			}
		}

		layers++;

		// create bit structures
		sizes = new u64[layers];
		datab = new u8* [layers];
		stopb = new SumBitArray* [layers];

		for (int i = 0; i < layers; i++) {
			u64 sizei = temp_sizes[i];
			sizes[i] = sizei;
			datab[i] = new u8[sizei];
			stopb[i] = new SumBitArray(sizei);
		}
	}

	void fillStructures(u64 n, u64* memblock) {
		// counters for indexing within the layers
		u64* counters = new u64[layers]();

		for (int i = 0; i < n / 8; i++) {
			u64 num = memblock[i];

			for (int j = 0; j < layers; j++) {
				u8 byte = num & 0x7F;

				// write the data bits
				datab[j][counters[j]] = byte;

				// write the stop bit
				if (num < 128) {
					stopb[j]->set(counters[j], 1);
					counters[j] += 1;
					break;
				} else {
					stopb[j]->set(counters[j], 0);
					counters[j] += 1;
				}

				num = num >> 7;
			}
		}

		delete[] counters;
	}

	void generateSums() {
		for (int i = 0; i < layers; i++) {
			stopb[i]->generateSums();
		}
	}

	u64 accessScan(u64 i) {
		u64 num = 0;
		u64 y = i;

		for (int j = 0; j < layers; j++) {
			u64 stopbit = stopb[j]->get(y);
			u8 byte = datab[j][y];
			// converted to u64 to avoid overflows on the left shift
			u64 byte64 = byte;

			num += byte64 << (7 * j);

			if (stopbit == 1) {
				return num;
			}
			
			y = (y - stopb[j]->sum(y));
		}

		// To save a whole byte of memory the 64th bit isn't actually saved.
		// Getting this far means the stop bit after the first 63 bits was 0,
		// so the 64th bit must be 1.

		u64 lastbit = 1;
		lastbit <<= 63;
		return num + lastbit;
	}

	u64 getSizeInBytes() {
		// layers value
		u64 size = 8;
		// sizes array
		size += layers * 8;

		for (int i = 0; i < layers; i++) {
			// datab array
			size += sizes[i];
			// stopb array
			size += stopb[i]->getSizeInBytes();
		}

		return size;
	}

	void dealloc() {
		for (int i = 0; i < layers; i++) {
			delete[] datab[i];
			stopb[i]->dealloc();
		}

		delete[] sizes;
		delete[] datab;
		delete[] stopb;	
	}
};

void benchmark(u64 size, LayeredVbyteArray a, u64 n) {
	vector<u64> ints;
	mt19937 g(random_device{}());
	uniform_int_distribution<u64> d(0, (size / 8) - 1);

	for (u64 i = 0; i < n; i++) {
		ints.push_back(d(g));
	}

	chrono::high_resolution_clock::time_point start = chrono::high_resolution_clock::now();

	for (int i : ints) {
		a.accessScan(i);
	}

	chrono::high_resolution_clock::time_point end = chrono::high_resolution_clock::now();
	chrono::duration<double> dur = chrono::duration_cast<chrono::duration<double>>(end - start);
	cout << fixed << setprecision(9);
	cout << n << " accessScan operations took: " << dur.count() << "s\n";
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
	LayeredVbyteArray a(size64, memblock);

	// testing
	cout << "class instance size: " << a.getSizeInBytes() << " bytes (" << a.getSizeInBytes() / 1048576 << "MB)\n";
	benchmark(size64, a, ops);

	delete[] memblock;
	a.dealloc();
	return 0;
}
