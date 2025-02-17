idf_component_register(SRCS "use_adc.c" 
                    INCLUDE_DIRS "."
                            "../common"
                    REQUIRES esp_adc_cal)