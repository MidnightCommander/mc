-- VHDL syntax highlighting sample file
-- Exercises all TS captures: keyword, string, keyword.other,
-- keyword.directive, tag, label, function, string.special

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

-- Entity with generic and port
entity counter is
    generic (
        WIDTH : natural := 8;
        MAX_VAL : natural := 255
    );
    port (
        clk    : in  std_logic;
        rst    : in  std_logic;
        enable : in  std_logic;
        count  : out std_logic_vector(WIDTH-1 downto 0);
        data   : inout std_logic_vector(7 downto 0);
        status : buffer std_logic);
end entity counter;

-- Architecture
architecture rtl of counter is
    signal cnt_reg : unsigned(WIDTH-1 downto 0);
    signal next_cnt : unsigned(WIDTH-1 downto 0);
    variable temp : integer := 0;
    constant ZERO : unsigned(WIDTH-1 downto 0)
        := (others => '0');
    constant ONE : unsigned(WIDTH-1 downto 0)
        := to_unsigned(1, WIDTH);

    -- Type and subtype declarations
    type state_type is (IDLE, RUNNING, DONE);
    subtype byte_t is std_logic_vector(7 downto 0);
    type mem_array is array (0 to 255) of byte_t;
    signal current_state : state_type;
    signal mem : mem_array;

    -- Alias and attribute
    alias cnt_msb : std_logic is cnt_reg(WIDTH-1);
    attribute syn_keep : boolean;
    attribute syn_keep of cnt_reg : signal is true;

    -- Component declaration
    component adder is
        generic (N : natural := 8);
        port (a : in unsigned(N-1 downto 0);
              b : in unsigned(N-1 downto 0);
              s : out unsigned(N-1 downto 0));
    end component adder;

    -- Function and procedure declarations
    function max_of(a : integer; b : integer)
        return integer is
    begin
        if a > b then return a;
        else return b; end if;
    end function max_of;

    procedure reset_counter(
        signal cnt : out unsigned(WIDTH-1 downto 0)
    ) is begin cnt <= ZERO;
    end procedure reset_counter;

begin

    -- Concurrent signal assignment with operators
    next_cnt <= cnt_reg + ONE;
    count <= std_logic_vector(cnt_reg);
    status <= '1';

    -- Process with sensitivity list
    clk_proc : process (clk, rst)
    begin
        if rst = '1' then
            cnt_reg <= ZERO;
            current_state <= IDLE;
        elsif rising_edge(clk) then
            if enable = '1' then
                cnt_reg <= next_cnt;
            end if;
        end if;
    end process clk_proc;

    -- State machine with case/when
    state_proc : process (clk)
    begin
        if rising_edge(clk) then
            case current_state is
                when IDLE => current_state <= RUNNING;
                when RUNNING => current_state <= DONE;
                when DONE => current_state <= IDLE;
                when others => current_state <= IDLE;
            end case;
        end if;
    end process state_proc;

    -- Word operators: not, and, or, nand, nor, xor, xnor
    data(0) <= not rst; data(1) <= clk and enable;
    data(2) <= clk or rst; data(3) <= clk nand enable;
    data(4) <= clk nor rst; data(5) <= clk xor enable;
    data(6) <= clk xnor rst;

    -- Generate statement with label
    gen_block : for i in 0 to WIDTH-1 generate
        data(i) <= cnt_reg(i);
    end generate gen_block;

    -- Component instantiation with port map
    add_inst : adder
        generic map (N => WIDTH)
        port map (a => cnt_reg, b => ONE, s => open);

    -- With/select concurrent assignment
    with current_state select data <=
        x"00" when IDLE, x"01" when RUNNING,
        x"FF" when DONE, x"AA" when others;

    -- Assert statement
    assert MAX_VAL > 0 report "positive" severity failure;

    -- Wait statement
    stim_proc : process
    begin
        wait until rising_edge(clk);
        wait for 10 ns; wait;
    end process stim_proc;

    -- Character/bit string literals, strings, concat
    data(7) <= 'Z'; mem(0) <= x"DEAD";
    mem(1) <= b"10101010"; mem(2) <= o"377";
    assert false report "Complete" severity note;
    data <= cnt_reg(3 downto 0) & "0000";

    -- Arithmetic: + - * / ** and comparison: = /= < >
    temp := max_of(WIDTH, 16);
    temp := temp + 1 - 1; temp := temp * 2 / 2;
    temp := 2 ** WIDTH;
    assert temp = 256 report "unexpected" severity warning;
    assert temp /= 0 report "zero" severity error;

    -- Block statement
    ctrl_block : block
    begin data <= (others => '0');
    end block ctrl_block;

    -- Additional: package body; configuration; impure;
    -- file f : text open; shared variable sv := 0;

end architecture rtl;
