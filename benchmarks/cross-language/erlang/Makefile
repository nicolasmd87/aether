.PHONY: all clean ping_pong

# Use standard Erlang compilation for fair baseline comparison
ERLC = erlc
ERLC_FLAGS =
ERL = erl
ERL_FLAGS = -noshell

all: ping_pong

ping_pong: ping_pong.erl
	$(ERLC) $(ERLC_FLAGS) $<

run_ping_pong: ping_pong
	$(ERL) $(ERL_FLAGS) -s ping_pong start

clean:
	rm -f *.beam

.DEFAULT_GOAL := all
