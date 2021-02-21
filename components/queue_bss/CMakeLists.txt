idf_component_register(SRCS "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_cleanup.c"
                            "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_dequeue.c"
                            "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_enqueue.c"
                            "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_init.c"
                            "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_internal.h"
                            "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer/lfds711_queue_bounded_singleproducer_singleconsumer_query.c"
                            "queue_bss.c"
                  INCLUDE_DIRS "."
                  PRIV_INCLUDE_DIRS "liblfds/liblfds7.1.1/liblfds711/inc"
                                    "liblfds/liblfds7.1.1/liblfds711/src/lfds711_queue_bounded_singleproducer_singleconsumer"
                                    "liblfds/liblfds7.1.1/liblfds711/src/lfds711_freelist"
                                    "liblfds/liblfds7.1.1/liblfds711/src/lfds711_misc"
                  LDFRAGMENTS linker.lf)

target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-unknown-pragmas"
                                                "-Wno-strict-aliasing"
                                                "-Wno-unused-but-set-variable")

# Should get us what we need without having to fork the lib!
target_compile_options(${COMPONENT_TARGET} PRIVATE "-D__linux__"
                                                   "-D__mips__")
