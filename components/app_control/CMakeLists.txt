idf_component_register(SRCS "app_control.c"
                    INCLUDE_DIRS "."  
                    "../common"
                    "../use_wifi"
                    "../use_uart"
                    "../protobuf" 
                    "../use_mfp"   
                    REQUIRES json nvs_flash bt mqtt)