package gopacket_lcm

import (
	"fmt"
	"sync"
	"time"

	"github.com/google/gopacket"
)

const (
	// Packages are declared invalid after this amount of seconds
	timeout time.Duration = 1 * time.Second
)

type lcmPacket struct {
	lastPacket time.Time
	done       bool
	totalFrags uint16
	frags      map[uint16]*LCMHeader
}

type LCMDefragmenter struct {
	mtx     *sync.Mutex
	packets map[uint32]*lcmPacket
}

func newLCMPacket(totalFrags uint16) *lcmPacket {
	return &lcmPacket{
		done:       false,
		totalFrags: totalFrags,
		frags:      make(map[uint16]*LCMHeader),
	}
}

func NewLCMDefragmenter() *LCMDefragmenter {
	return &LCMDefragmenter{
		mtx:     &sync.Mutex{},
		packets: make(map[uint32]*lcmPacket),
	}
}

func (lp *lcmPacket) append(in *LCMHeader) {
	lp.frags[in.FragmentNumber] = in
	lp.lastPacket = time.Now()
}

func (lp *lcmPacket) assemble() (out *LCMHeader, err error) {
	var blob []byte

	//Extract packets
	for i := uint16(0); i < lp.totalFrags; i++ {
		fragment, ok := lp.frags[i]
		if !ok {
			err = fmt.Errorf("Tried to defragment incomplete packet. Waiting "+
				"for more potential (unordered) packets... %d", i)
			return
		}

		// For the very first packet, we also want the header.
		if i == 0 {
			blob = append(blob, fragment.contents...)
		}

		// Append the data for each packet.
		blob = append(blob, fragment.payload...)
	}

	packet := gopacket.NewPacket(blob, LayerTypeLCMHeader, gopacket.NoCopy)
	lcmHdrLayer := packet.Layer(LayerTypeLCMHeader)
	out, ok := lcmHdrLayer.(*LCMHeader)
	if !ok {
		err = fmt.Errorf("Error while decoding the defragmented packet. " +
			"Erasing/dropping packet.")
	}

	lp.done = true

	return
}

func (ld *LCMDefragmenter) cleanUp() {
	for key, packet := range ld.packets {
		if packet.done || time.Now().Sub(packet.lastPacket) > timeout {
			delete(ld.packets, key)
		}
	}
}

func (ld *LCMDefragmenter) Defrag(in *LCMHeader) (out *LCMHeader, err error) {
	// Timeout old packages and erase error prone ones.
	ld.cleanUp()

	// Quick check if this is acutally a single packet. In that case, just
	// return it quickly.
	if !in.Fragmented {
		out = in
		return
	}

	// Do we need to start a new fragments obj?
	if _, ok := ld.packets[in.SequenceNumber]; !ok {
		ld.packets[in.SequenceNumber] = newLCMPacket(in.TotalFragments)
	}

	// Append the packet
	ld.packets[in.SequenceNumber].append(in)

	// Check if this is the last package of that series
	if in.FragmentNumber == in.TotalFragments-1 {
		out, err = ld.packets[in.SequenceNumber].assemble()
	}

	return
}
