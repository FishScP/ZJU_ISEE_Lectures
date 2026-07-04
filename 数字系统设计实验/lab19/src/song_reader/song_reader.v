//  song_reader.v
//    结束条件：地址计数器进位 或 duration==0


module song_reader (
    input  wire       clk,
    input  wire       reset,
    input  wire       play,
    input  wire [1:0] song,
    input  wire       note_done,
    output wire       song_done,
    output wire [5:0] note,
    output wire [5:0] duration,
    output wire       new_note
);
    wire [4:0]  addr_q;
    wire        addr_carry;
    wire [6:0]  rom_addr;
    wire [11:0] rom_dout;
    wire        addr_clr, addr_en;

    // 1. 地址计数器
    addr_counter u_addr_cnt (
        .clk   (clk),
        .clr   (addr_clr),
        .en    (addr_en),
        .q     (addr_q),
        .carry (addr_carry)
    );

    // 2. song_rom
    assign rom_addr = {song, addr_q};
    song_rom u_rom (
        .clk  (clk),
        .addr (rom_addr),
        .dout (rom_dout)
    );
    assign note     = rom_dout[11:6];
    assign duration = rom_dout[5:0];

    // 3. 结束检测器（检测进位 或 duration==0）
    end_detector u_end_det (
        .clk         (clk),
        .reset       (reset),
        .play        (play),
        .addr_carry  (addr_carry),
        .duration    (duration),
        .song_done   (song_done)
    );

    // 4. 控制器 FSM
    sr_controller u_sr_ctrl (
        .clk       (clk),
        .reset     (reset),
        .play      (play),
        .note_done (note_done),
        .song_done (song_done),
        .addr_clr  (addr_clr),
        .addr_en   (addr_en),
        .new_note  (new_note)
    );
endmodule

//  addr_counter.v
module addr_counter (
    input  wire       clk,
    input  wire       clr,
    input  wire       en,
    output reg  [4:0] q,
    output reg        carry
);
    always @(posedge clk) begin
        if (clr) begin
            q <= 5'b00000;
            carry <= 1'b0;
        end else if (en) begin
            if (q == 5'd31) begin
                q <= 5'b00000;
                carry <= 1'b1;
            end else begin
                q <= q + 5'b00001;
                carry <= 1'b0;
            end
        end else begin
            carry <= 1'b0;
        end
    end
endmodule

//  end_detector.v 
//    当 play=1 且 (addr_carry=1 或 duration==0) 时，
//    产生一个时钟周期的 song_done 脉冲
module end_detector (
    input  wire       clk,
    input  wire       reset,
    input  wire       play,
    input  wire       addr_carry,
    input  wire [5:0] duration,
    output reg        song_done
);
    localparam IDLE = 1'b0, FIRED = 1'b1;
    reg state;
    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            song_done <= 1'b0;
        end else begin
            case (state)
                IDLE: begin
                    if (play && (addr_carry || (duration == 6'd0))) begin
                        song_done <= 1'b1;
                        state <= FIRED;
                    end else begin
                        song_done <= 1'b0;
                    end
                end
                FIRED: begin
                    song_done <= 1'b0;
                    if (!(addr_carry || (duration == 6'd0)))
                        state <= IDLE;
                end
                default: begin state <= IDLE; song_done <= 1'b0; end
            endcase
        end
    end
endmodule

//  sr_controller.v
//    状态：RESET -> NEW_NOTE -> WAIT -> NEXT_NOTE -> NEW_NOTE
module sr_controller (
    input  wire clk, reset,
    input  wire play, note_done, song_done,
    output reg  addr_clr, addr_en, new_note
);
    localparam [1:0] S_RESET=0, S_NEW_NOTE=1, S_WAIT=2, S_NEXT_NOTE=3;
    reg [1:0] state, next;
    always @(posedge clk) begin
        if (reset) state <= S_RESET;
        else state <= next;
    end
    always @(*) begin
        next = state;
        case (state)
            S_RESET:     if (play) next = S_NEW_NOTE;
            S_NEW_NOTE:  next = S_WAIT;
            S_WAIT:      if (!play || song_done) next = S_RESET;
                         else if (note_done) next = S_NEXT_NOTE;
            S_NEXT_NOTE: next = S_NEW_NOTE;
            default:     next = S_RESET;
        endcase
    end
    always @(*) begin
        addr_clr = 1'b0; addr_en = 1'b0; new_note = 1'b0;
        case (state)
            S_RESET:     addr_clr = 1'b1;
            S_NEW_NOTE:  new_note = 1'b1;
            S_WAIT:      ;
            S_NEXT_NOTE: addr_en = 1'b1;
            default:     addr_clr = 1'b1;
        endcase
    end
endmodule