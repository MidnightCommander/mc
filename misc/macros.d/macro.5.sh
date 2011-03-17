#silent
s       snippets
        a=`cat %b`
        if [ "$a" = "fori" ]; then
        echo "for (i = 0; i _; i++)" > %b
        fi
        if [ "$a" = "ife" ]; then
        cat <<EOF > %b
        if ( )
        {
        }
        else
        {
        }
        EOF
        fi
        if [ "$a" = "GPL" ]; then
        cat >>%b <<EOF
        /*
        * This program is free software; you can redistribute it and/or modify
        * it under the terms of the GNU General Public License as published by
        * the Free Software Foundation; either version 2 of the License, or
        * (at your option) any later version.
        *
        * This program is distributed in the hope that it will be useful,
        * but WITHOUT ANY WARRANTY; without even the implied warranty of
        * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
        * GNU General Public License for more details.
        *
        * You should have received a copy of the GNU General Public License
        * along with this program; if not, write to the Free Software
        * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
        */
        EOF
        fi
        if [ "$a" = "type" ]; then
        cat <<EOF > %b
        typedef struct {
        ;
        } ?;
        EOF
        fi
