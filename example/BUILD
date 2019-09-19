#    Distributed Vector-OLE Generator
#    Copyright (C) 2019 Phillipp Schoppmann and Adria Gascon
#
#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU Affero General Public License as
#    published by the Free Software Foundation, either version 3 of the
#    License, or (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU Affero General Public License for more details.
#
#    You should have received a copy of the GNU Affero General Public License
#    along with this program.  If not, see <https://www.gnu.org/licenses/>.

cc_binary(
    name = "example",
    srcs = [
        "example.cpp",
    ],
    copts = [
        "-fopenmp",
        "-Wno-ignored-attributes",
    ],
    deps = [
        "@distributed_vector_ole//distributed_vector_ole",
        "@mpc_utils//mpc_utils:comm_channel",
        "@mpc_utils//mpc_utils:config",
    ],
)
