#include <gb/gb.hpp>

#define CARTRIDGE_CPP
namespace GameBoy {

#include "mbc0/mbc0.cpp"
#include "mbc1/mbc1.cpp"
#include "mbc2/mbc2.cpp"
#include "mbc3/mbc3.cpp"
#include "mbc5/mbc5.cpp"
#include "mmm01/mmm01.cpp"
#include "huc1/huc1.cpp"
#include "huc3/huc3.cpp"
#include "serialization.cpp"
Cartridge cartridge;

string Cartridge::title() {
  return information.title;
}

//intended for use with Super Game Boy for when no Game Boy cartridge is inserted
void Cartridge::load_empty(System::Revision revision) {
  unload();
  romsize = 32768;
  romdata = allocate<uint8>(romsize, 0xff);
  ramsize = 0;
  mapper = &mbc0;
  sha256 = Hash::SHA256(romdata, romsize).digest();
  loaded = true;
  system.load(revision);
}

void Cartridge::load(System::Revision revision) {
  unload();

  system.revision = revision;  //needed for ID::Manifest to return correct group ID
  if(revision != System::Revision::SuperGameBoy) {
    interface->loadRequest(ID::Manifest, "manifest.bml", true);
  }

  information.mapper = Mapper::Unknown;
  information.ram = false;
  information.battery = false;
  information.rtc = false;
  information.rumble = false;

  information.romsize = 0;
  information.ramsize = 0;

  auto document = BML::unserialize(information.markup);
  information.title = document["information/title"].text();

  auto mapperid = document["cartridge/board/type"].text();
  if(mapperid == "none" ) information.mapper = Mapper::MBC0;
  if(mapperid == "MBC1" ) information.mapper = Mapper::MBC1;
  if(mapperid == "MBC2" ) information.mapper = Mapper::MBC2;
  if(mapperid == "MBC3" ) information.mapper = Mapper::MBC3;
  if(mapperid == "MBC5" ) information.mapper = Mapper::MBC5;
  if(mapperid == "MMM01") information.mapper = Mapper::MMM01;
  if(mapperid == "HuC1" ) information.mapper = Mapper::HuC1;
  if(mapperid == "HuC3" ) information.mapper = Mapper::HuC3;

  information.rtc = false;
  information.rumble = false;

  auto rom = document["cartridge/rom"];
  auto ram = document["cartridge/ram"];

  romsize = rom["size"].decimal();
  romdata = allocate<uint8>(romsize, 0xff);

  ramsize = ram["size"].decimal();
  ramdata = allocate<uint8>(ramsize, 0xff);

  //Super Game Boy core loads memory from Super Famicom core
  if(revision != System::Revision::SuperGameBoy) {
    if(auto name = rom["name"].text()) interface->loadRequest(ID::ROM, name, true);
    if(auto name = ram["name"].text()) interface->loadRequest(ID::RAM, name, false);
    if(auto name = ram["name"].text()) memory.append({ID::RAM, name});
  }

  information.romsize = rom["size"].decimal();
  information.ramsize = ram["size"].decimal();
  information.battery = (bool)ram["name"];

  switch(information.mapper) { default:
  case Mapper::MBC0:  mapper = &mbc0;  break;
  case Mapper::MBC1:  mapper = &mbc1;  break;
  case Mapper::MBC2:  mapper = &mbc2;  break;
  case Mapper::MBC3:  mapper = &mbc3;  break;
  case Mapper::MBC5:  mapper = &mbc5;  break;
  case Mapper::MMM01: mapper = &mmm01; break;
  case Mapper::HuC1:  mapper = &huc1;  break;
  case Mapper::HuC3:  mapper = &huc3;  break;
  }

  sha256 = Hash::SHA256(romdata, romsize).digest();
  loaded = true;
  system.load(revision);
}

void Cartridge::unload() {
  if(romdata) { delete[] romdata; romdata = nullptr; romsize = 0; }
  if(ramdata) { delete[] ramdata; ramdata = nullptr; ramsize = 0; }
  loaded = false;
}

uint8 Cartridge::rom_read(unsigned addr) {
  if(addr >= romsize) addr %= romsize;
  return romdata[addr];
}

void Cartridge::rom_write(unsigned addr, uint8 data) {
  if(addr >= romsize) addr %= romsize;
  romdata[addr] = data;
}

uint8 Cartridge::ram_read(unsigned addr) {
  if(ramsize == 0) return 0x00;
  if(addr >= ramsize) addr %= ramsize;
  return ramdata[addr];
}

void Cartridge::ram_write(unsigned addr, uint8 data) {
  if(ramsize == 0) return;
  if(addr >= ramsize) addr %= ramsize;
  ramdata[addr] = data;
}

uint8 Cartridge::mmio_read(uint16 addr) {
  if(addr == 0xff50) return 0x00;

  if(bootrom_enable) {
    const uint8* data = nullptr;
    switch(system.revision) { default:
    case System::Revision::GameBoy: data = system.bootROM.dmg; break;
    case System::Revision::SuperGameBoy: data = system.bootROM.sgb; break;
    case System::Revision::GameBoyColor: data = system.bootROM.cgb; break;
    }
    if(addr >= 0x0000 && addr <= 0x00ff) return data[addr];
    if(addr >= 0x0200 && addr <= 0x08ff && system.cgb()) return data[addr - 256];
  }

  return mapper->mmio_read(addr);
}

void Cartridge::mmio_write(uint16 addr, uint8 data) {
  if(bootrom_enable && addr == 0xff50) {
    bootrom_enable = false;
    return;
  }

  mapper->mmio_write(addr, data);
}

void Cartridge::power() {
  bootrom_enable = true;

  mbc0.power();
  mbc1.power();
  mbc2.power();
  mbc3.power();
  mbc5.power();
  mmm01.power();
  huc1.power();
  huc3.power();

  for(unsigned n = 0x0000; n <= 0x7fff; n++) bus.mmio[n] = this;
  for(unsigned n = 0xa000; n <= 0xbfff; n++) bus.mmio[n] = this;
  bus.mmio[0xff50] = this;
}

Cartridge::Cartridge() {
  loaded = false;
  sha256 = "";
}

Cartridge::~Cartridge() {
  unload();
}

}
