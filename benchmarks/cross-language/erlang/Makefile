.PHONY: all clean ping_pong ring skynet

# Use native compilation with HiPE optimization for best performance
ERLC = erlc
ERLC_FLAGS = +native +'{hipe, [o3]}'
ERL = erl
ERL_FLAGS = -noshell

MODULES = ping_pong ring skynet

all: $(MODULES)

ping_pong: ping_pong.erl
	$(ERLC) $(ERLC_FLAGS) $<

ring: ring.erl
	$(ERLC) $(ERLC_FLAGS) $<

skynet: skynet.erl
	$(ERLC) $(ERLC_FLAGS) $<

run_ping_pong: ping_pong
	$(ERL) $(ERL_FLAGS) -s ping_pong start

run_ring: ring
	$(ERL) $(ERL_FLAGS) -s ring start

run_skynet: skynet
	$(ERL) $(ERL_FLAGS) -s skynet start

clean:
	rm -f *.beam

.DEFAULT_GOAL := all
