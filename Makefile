CXX ?= c++
CXXFLAGS ?= -std=c++20 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS += -Iinclude

LIB_SOURCES = src/lexer.cpp src/translator.cpp src/sdag.cpp src/ci_emitter.cpp src/cpp_emitter.cpp
HEADERS = src/lexer.hpp src/sdag.hpp src/semantic.hpp src/ci_emitter.hpp src/cpp_emitter.hpp include/charmsharp/translator.hpp

.PHONY: all test charm-check charm-run charm-objects charm-threaded charm-message charm-readonly charm-inheritance charm-message-inheritance charm-parallel-inheritance charm-benchmark-primes charm-benchmark-reduction charm-ctor-initializers charm-benchmark-jacobi2d charm-marshalled-arrays charm-sdag-when charm-sdag-sequence charm-sdag-control charm-benchmark-prefix-sum charm-sdag-extended charm-implicit-main charm-instrumentation-attributes charm-ownership-exclusive charm-direct-call charm-template-declaration charm-template-registration charm-template-auto-registration charm-template-multi-specialization charm-template-cross-tu charm-template-header-order charm-variable-message charm-readonly-message charm-init-hooks charm-pupable clean
all: build/charmsharp

build/charmsharp: src/main.cpp $(LIB_SOURCES) $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/main.cpp $(LIB_SOURCES) -o $@

build/tests: tests/tests.cpp $(LIB_SOURCES) $(HEADERS)
	mkdir -p build
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/tests.cpp $(LIB_SOURCES) -o $@

test: build/tests build/charmsharp
	./build/tests
	sh tests/cli_smoke.sh ./build/charmsharp examples/Counter.csh
	sh tests/build_cli.sh ./build/charmsharp examples/Hello.csh
	sh tests/multifile_cli.sh ./build/charmsharp

charm-check: build/charmsharp
	sh tests/charm_smoke.sh ./build/charmsharp examples/Counter.csh

charm-run: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Hello.csh

charm-objects: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Objects.csh Objects 'Hello from array Cell'

charm-threaded: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Threaded.csh Threaded 'Hello from threaded entry'

charm-message: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Message.csh Message 'Hello from message: 42'

charm-readonly: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Readonly.csh Readonly 'Hello from readonly: 42'

charm-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/Inheritance.csh Inheritance 'Hello from inherited base: 42'

charm-message-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/MessageInheritance.csh MessageInheritance 'Hello from inherited message: 42'

charm-parallel-inheritance: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/ParallelInheritance.csh ParallelInheritance 'Hello from inherited proxy: 42'

charm-benchmark-primes: build/charmsharp
	sh tests/benchmark_primes.sh ./build/charmsharp

charm-benchmark-reduction: build/charmsharp
	sh tests/benchmark_reduction.sh ./build/charmsharp

charm-ctor-initializers: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/ConstructorInitializers.csh ConstructorInitializers 'Hello from constructor initializers: 43'

charm-benchmark-jacobi2d: build/charmsharp
	sh tests/benchmark_jacobi2d.sh ./build/charmsharp

charm-marshalled-arrays: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/MarshalledArrays.csh MarshalledArrays 'Hello from marshalled arrays: 6'

charm-sdag-when: build/charmsharp
	sh tests/sdag_when_integration.sh ./build/charmsharp

charm-sdag-sequence: build/charmsharp
	sh tests/sdag_sequence_integration.sh ./build/charmsharp

charm-sdag-control: build/charmsharp
	sh tests/sdag_control_integration.sh ./build/charmsharp

charm-benchmark-prefix-sum: build/charmsharp
	sh tests/benchmark_prefix_sum.sh ./build/charmsharp

charm-sdag-extended: build/charmsharp
	sh tests/sdag_extended_integration.sh ./build/charmsharp

charm-implicit-main: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/ImplicitMain.csh ImplicitMain 'Hello from Charm#'

charm-instrumentation-attributes: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/InstrumentationAttributes.csh InstrumentationAttributes 'Instrumentation attributes: 42'

charm-ownership-exclusive: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/OwnershipExclusive.csh OwnershipExclusive 'Ownership and exclusion: 42'

charm-direct-call: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/DirectCallAttributes.csh DirectCallAttributes 'Direct-call attributes: 42'

charm-template-declaration: build/charmsharp
	sh tests/charm_smoke.sh ./build/charmsharp examples/TemplateDeclaration.csh

charm-template-registration: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/TemplateRegistration.csh TemplateRegistration 'Template worker: 42'

charm-template-auto-registration: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/TemplateAutoRegistration.csh TemplateAutoRegistration 'Template worker: 42'

charm-template-multi-specialization: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/TemplateMultiSpecialization.csh TemplateMultiSpecialization 'Both template specializations completed'

charm-template-cross-tu: build/charmsharp
	sh tests/charm_cross_tu.sh ./build/charmsharp examples/CrossTU.csh examples/CrossTU_helper.cpp

charm-template-header-order: build/charmsharp
	sh tests/charm_run.sh ./build/charmsharp examples/TemplateHeaderOrder.csh TemplateHeaderOrder 'Ordered template chares completed'

clean:
	rm -f build/charmsharp build/tests


charm-variable-message: build/charmsharp
	sh tests/variable_message_integration.sh ./build/charmsharp

charm-readonly-message: build/charmsharp
	sh tests/readonly_message_integration.sh ./build/charmsharp

charm-init-hooks: build/charmsharp
	sh tests/init_hooks_integration.sh ./build/charmsharp

charm-pupable: build/charmsharp
	sh tests/pupable_integration.sh ./build/charmsharp
