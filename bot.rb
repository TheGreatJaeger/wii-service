require 'discordrb'

bot = Discordrb::Bot.new token: 'BOT_TOKEN'

bot.message(starting_with: 'wii$') do |event|
  command_string = event.message.content.strip
  command_string = command_string.sub(/^wii\$/, '')
  parts = command_string.split(' ', 2)
  if parts.empty?
    event.respond('No command provided!')
  else
    command = parts[0]
    arg = (parts.length() == 2) ? parts[1] : ""

    if command == 'help'
      help = "`wii$help`- Run this help dialog\n"
      help += "`wii$neofetch`- Run neofetch and return the results\n"
      help += "`wii$uname` - Runs uname and sends the results\n"
      help += "`wii$say`- Send a message and display it on Jaeger\'s monitors\n"
      help += "`wii$draw`- Draw some graphics on Jaeger\'s monitors"
      event.respond(help)
    elsif command == 'say'
      print event.user.name,": ", arg, "\n"
      event.respond("Message sent!")
    elsif command == 'draw'
      arg = arg.gsub("'", "'\\''")
      fbdraw = `./fbdraw /dev/fb0 '#{arg}' 2>&1`
      fbdraw = fbdraw.gsub("`", "\u200b`")
      event.respond((fbdraw.empty?) ? "Drawing sent!" : "```ansi\n" + fbdraw + "\n```")
    elsif command == 'uname'
      uname = `uname -a`
      event.respond(uname)
    elsif command == 'neofetch'
      neofetch = `neofetch`
      neofetch = neofetch.gsub(/\x1b(?:[@-Z\\-_]|\[(?!([0-9]|[2-4][0-9])?m)[0-?]*[ -\/]*[@-~])/, "")
      neofetch = neofetch.gsub("\x1b[m", "\x1b[0m")
      neofetch = neofetch.gsub("`", "\u200b`")
      event.respond("```ansi\n" + neofetch + "\n```")
    elsif command == 'clear'
      print "\x1b[H\x1b[2J\x1b[3J"
      event.respond('Screen cleared!!')
    else
      event.respond("Unknown command: #{command}")
    end
  end
end

bot.run
