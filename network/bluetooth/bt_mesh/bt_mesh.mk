NAME := bt_mesh

$(NAME)_MBINS_TYPE := kernel
$(NAME)_VERSION := 1.0.0
$(NAME)_SUMMARY := BLE Mesh stack.

ifeq ($(bt_mesh_standalone_deploy),1)
GLOBAL_DEFINES += CONFIG_MESH_STACK_ALONE
$(NAME)_COMPONENTS += bluetooth.bt_mesh.util
else
$(NAME)_COMPONENTS += bluetooth.bt_common 
endif

$(NAME)_COMPONENTS += bluetooth.bt_mesh.ref_impl

$(NAME)_INCLUDES += ./inc/ \
                    ./inc/api/ \
                    ./inc/api/mesh/ \
                    ../../../genie_app/ \
					../../../genie_app/base \
					../../../genie_app/bluetooth/host \
					../../../genie_app/bluetooth/mesh

                    
$(NAME)_SOURCES  :=  ./src/access.c \
                     ./src/adv.c \
                     ./src/beacon.c \
                     ./src/crypto.c \
                     ./src/cfg_srv.c \
                     ./src/cfg_cli.c \
                     ./src/health_srv.c \
                     ./src/health_cli.c \
                     ./src/main.c \
                     ./src/net.c \
                     ./src/proxy.c \
                     ./src/transport.c \
                     ./src/friend.c \
                     ./src/lpn.c \
                     ./src/shell.c 

ifeq ($(FUNC_BT_MESH_PROVISIONER), y)
$(NAME)_SOURCES  += ./src/provisioner_prov.c # 配网器 ./src/provisioner_prov.c
else
$(NAME)_SOURCES  += ./src/prov.c             # 节点   ./src/prov.c 
endif

GLOBAL_INCLUDES += ./inc/ \
                   ./inc/api


GLOBAL_DEFINES += CRC16_ENABLED
